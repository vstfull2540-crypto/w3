#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>
#include <cstdint>

VUMisterNAMAudioProcessor::VUMisterNAMAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout()),
      modelLoader (std::make_unique<NeuralAudio::NeuralModelLoader>())
{
    // Allocate enough model history for typical DAW block sizes before any audio runs.
    modelLoader->SetDefaultMaxAudioBufferSize (8192);
}

juce::AudioProcessorValueTreeState::ParameterLayout
VUMisterNAMAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"input", 1}, "Input", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, " dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"output", 1}, "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, " dB"));
    layout.add (std::make_unique<juce::AudioParameterFloat>(juce::ParameterID {"mix", 1}, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));
    layout.add (std::make_unique<juce::AudioParameterBool>(juce::ParameterID {"bypass", 1}, "Bypass", false));
    return layout;
}

void VUMisterNAMAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    modelScratch.setSize (1, juce::jmax (8192, samplesPerBlock), false, false, true);
    // NeuralAudio applies its external sample rate when loading, so reload the selected model
    // after a sample-rate change. Model construction is kept off the audio callback.
    if (namPath.isNotEmpty())
        loadNamFile (juce::File (namPath));
}

void VUMisterNAMAudioProcessor::releaseResources() {}

bool VUMisterNAMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo()) && in == out;
}

void VUMisterNAMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int samples = buffer.getNumSamples();
    for (int ch = getTotalNumOutputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, samples);

    // Optional audio-file audition source. The file is decoded on the UI thread;
    // this callback only reads an immutable buffer and performs linear resampling.
    const auto audioSource = std::atomic_load (&loadedAudio);
    if (audioSource && audioSource->samples.getNumSamples() > 0 && currentSampleRate > 0.0)
    {
        const int sourceLength = audioSource->samples.getNumSamples();
        const int sourceChannels = audioSource->samples.getNumChannels();
        const double increment = audioSource->sampleRate / currentSampleRate;
        double position = audioPlaybackPosition.load (std::memory_order_relaxed);
        position = std::fmod (position, static_cast<double> (sourceLength));
        for (int i = 0; i < samples; ++i)
        {
            const auto index0 = static_cast<int> (position);
            const auto index1 = (index0 + 1) % sourceLength;
            const float fraction = static_cast<float> (position - index0);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                const int sourceChannel = juce::jmin (ch, sourceChannels - 1);
                const float a = audioSource->samples.getSample (sourceChannel, index0);
                const float b = audioSource->samples.getSample (sourceChannel, index1);
                buffer.setSample (ch, i, a + (b - a) * fraction);
            }
            position += increment;
            if (position >= sourceLength)
                position = std::fmod (position, static_cast<double> (sourceLength));
        }
        audioPlaybackPosition.store (position, std::memory_order_relaxed);
    }

    double inputEnergy = 0.0, outputEnergy = 0.0;
    int meterSampleCount = 0;
    const float inGain = juce::Decibels::decibelsToGain (parameters.getRawParameterValue ("input")->load());
    const float outGain = juce::Decibels::decibelsToGain (parameters.getRawParameterValue ("output")->load());
    const float mix = parameters.getRawParameterValue ("mix")->load() * 0.01f;
    const bool bypass = parameters.getRawParameterValue ("bypass")->load() > 0.5f;
    // Atomic shared_ptr load keeps the old model alive until the audio block has finished.
    const auto active = std::atomic_load (&models);

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < samples; ++i)
        {
            const double sample = static_cast<double> (data[i]);
            inputEnergy += sample * sample;
            if (! bypass)
                data[i] *= inGain;
        }

        if (! bypass && active && active->left)
        {
            auto model = (ch == 0 || ! active->right) ? active->left : active->right;
            int offset = 0;
            while (offset < samples)
            {
                const int count = juce::jmin (modelScratch.getNumSamples(), samples - offset);
                auto* input = data + offset;
                auto* wet = modelScratch.getWritePointer (0);
                model->Process (input, wet, count);
                for (int i = 0; i < count; ++i)
                    input[i] = input[i] * (1.0f - mix) + wet[i] * mix;
                offset += count;
            }
        }

        for (int i = 0; i < samples; ++i)
        {
            // True bypass means the entire effect is out of circuit: input gain,
            // NAM inference, dry/wet blend, and output gain are all skipped.
            // This ensures toggling BYPASS produces an exact input passthrough.
            if (! bypass)
                data[i] *= outGain;

            const double sample = static_cast<double> (data[i]);
            outputEnergy += sample * sample;
        }
        meterSampleCount += samples;
    }
    // Meter ballistics are calculated on the audio thread so short transients are
    // not lost between GUI timer ticks. Attack is fast; release is deliberately
    // slower to make the analogue needles readable at 30 Hz.
    const double divisor = static_cast<double> (juce::jmax (1, meterSampleCount));
    const float inputRms = static_cast<float> (std::sqrt (inputEnergy / divisor));
    const float outputRms = static_cast<float> (std::sqrt (outputEnergy / divisor));
    const float targetInputDb  = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (inputRms, -60.0f));
    const float targetOutputDb = juce::jmax (-60.0f, juce::Decibels::gainToDecibels (outputRms, -60.0f));
    const float blockSeconds = (currentSampleRate > 0.0)
        ? static_cast<float> (samples / currentSampleRate) : 0.01f;

    const auto smoothMeter = [blockSeconds] (float previousDb, float targetDb)
    {
        constexpr float attackSeconds = 0.025f;
        constexpr float releaseSeconds = 0.280f;
        const float timeConstant = (targetDb > previousDb) ? attackSeconds : releaseSeconds;
        const float coefficient = std::exp (-blockSeconds / timeConstant);
        return targetDb + coefficient * (previousDb - targetDb);
    };

    const float previousInputDb = inputMeterDb.load (std::memory_order_relaxed);
    const float previousOutputDb = outputMeterDb.load (std::memory_order_relaxed);
    inputMeterDb.store (smoothMeter (previousInputDb, targetInputDb), std::memory_order_relaxed);
    outputMeterDb.store (smoothMeter (previousOutputDb, targetOutputDb), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* VUMisterNAMAudioProcessor::createEditor() { return new VUMisterNAMAudioProcessorEditor (*this); }

void VUMisterNAMAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("namPath", namPath, nullptr);
    state.setProperty ("audioPath", audioPath, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void VUMisterNAMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            parameters.replaceState (state);
            namPath = state.getProperty ("namPath").toString();
            audioPath = state.getProperty ("audioPath").toString();
            if (namPath.isNotEmpty() && juce::File (namPath).existsAsFile()) loadNamFile (juce::File (namPath));
            else if (namPath.isNotEmpty()) namStatus = "Saved NAM file not found";
            if (audioPath.isNotEmpty() && juce::File (audioPath).existsAsFile()) loadAudioFile (juce::File (audioPath));
            else if (audioPath.isNotEmpty()) audioStatus = "Saved audio file not found; using DAW input";
        }
}

juce::String VUMisterNAMAudioProcessor::getNamPath() const { return namPath; }
juce::String VUMisterNAMAudioProcessor::getNamStatus() const { return namStatus; }

bool VUMisterNAMAudioProcessor::loadNamFile (const juce::File& file)
{
    const auto extension = file.getFileExtension();
    const bool isNamModel = extension.compareIgnoreCase (".nam") == 0;
    const bool isNamA2Model = extension.compareIgnoreCase (".gen") == 0;
    if (! file.existsAsFile() || (! isNamModel && ! isNamA2Model))
    {
        namStatus = "Invalid model file: choose .nam (NAM) or .gen (NAM A2)";
        return false;
    }

    try
    {
        modelLoader->SetExternalSampleRate (currentSampleRate);
        auto candidate = std::make_shared<LoadedModels>();
        auto* left = modelLoader->CreateFromFile (file.getFullPathName().toStdString());
        if (left == nullptr) throw std::runtime_error ("NeuralAudio could not create the left-channel model");
        candidate->left = std::shared_ptr<NeuralAudio::NeuralModel> (left);

        // A second model instance is required for stereo because the model has internal state.
        auto* right = modelLoader->CreateFromFile (file.getFullPathName().toStdString());
        if (right == nullptr) throw std::runtime_error ("NeuralAudio could not create the right-channel model");
        candidate->right = std::shared_ptr<NeuralAudio::NeuralModel> (right);

        std::atomic_store (&models, candidate);
        // Selecting a model stops file playback and returns processing to DAW input.
        clearAudioFile();
        namPath = file.getFullPathName();
        namStatus = isNamA2Model ? "NAM A2 model loaded and running" : "NAM model loaded and running";
        return true;
    }
    catch (const std::exception& e)
    {
        namStatus = "NAM load failed: " + juce::String (e.what());
    }
    catch (...)
    {
        namStatus = "NAM load failed: unknown NeuralAudio error";
    }
    return false;
}


juce::String VUMisterNAMAudioProcessor::getAudioPath() const { return audioPath; }
juce::String VUMisterNAMAudioProcessor::getAudioStatus() const { return audioStatus; }

void VUMisterNAMAudioProcessor::clearAudioFile()
{
    std::atomic_store (&loadedAudio, std::shared_ptr<LoadedAudioFile>{});
    audioPlaybackPosition.store (0.0, std::memory_order_relaxed);
    audioPath.clear();
    audioStatus = "Using DAW input";
}

bool VUMisterNAMAudioProcessor::loadAudioFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        audioStatus = "Audio load failed: file not found";
        return false;
    }
    if (file.getFileExtension().compareIgnoreCase (".wav") != 0)
    {
        audioStatus = "Unsupported audio file: choose .wav only";
        return false;
    }

    try
    {
        juce::AudioFormatManager formats;
        formats.registerFormat (new juce::WavAudioFormat(), true);
        auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));
        if (reader == nullptr)
        {
            audioStatus = "WAV load failed: file is not a valid supported WAV stream";
            return false;
        }
        if (reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        {
            audioStatus = "Audio load failed: invalid audio stream";
            return false;
        }
        // Decode at most 512 MiB of float PCM to keep accidental huge files from
        // exhausting the host process. Only the first two channels are used.
        const auto length = reader->lengthInSamples;
        const int channels = static_cast<int> (juce::jlimit (1, 2, static_cast<int> (reader->numChannels)));
        constexpr std::int64_t maxFloatSamples = (512LL * 1024LL * 1024LL) / static_cast<std::int64_t> (sizeof (float));
        if (length > maxFloatSamples / channels || length > std::numeric_limits<int>::max())
        {
            audioStatus = "Audio load failed: file is too large (512 MiB decoded limit)";
            return false;
        }

        auto candidate = std::make_shared<LoadedAudioFile>();
        candidate->samples.setSize (channels, static_cast<int> (length));
        candidate->samples.clear();
        const bool readOk = reader->read (&candidate->samples, 0, static_cast<int> (length), 0, true, channels > 1);
        if (! readOk)
        {
            audioStatus = "Audio load failed while decoding the file";
            return false;
        }
        candidate->sampleRate = reader->sampleRate;
        candidate->path = file.getFullPathName();
        std::atomic_store (&loadedAudio, candidate);
        audioPlaybackPosition.store (0.0, std::memory_order_relaxed);
        audioPath = candidate->path;
        audioStatus = "WAV audio source loaded (looping)";
        return true;
    }
    catch (const std::exception& e)
    {
        audioStatus = "Audio load failed: " + juce::String (e.what());
    }
    catch (...)
    {
        audioStatus = "Audio load failed: unknown decoder error";
    }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VUMisterNAMAudioProcessor(); }
