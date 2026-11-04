#pragma once
#include <JuceHeader.h>
#include <NeuralAudio/NeuralModel.h>
#include <atomic>
#include <memory>

class VUMisterNAMAudioProcessor final : public juce::AudioProcessor
{
public:
    VUMisterNAMAudioProcessor();
    ~VUMisterNAMAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    float getInputMeterDb() const noexcept { return inputMeterDb.load(); }
    float getOutputMeterDb() const noexcept { return outputMeterDb.load(); }
    juce::String getNamPath() const;
    juce::String getNamStatus() const;
    bool loadNamFile (const juce::File& file);
    juce::String getAudioPath() const;
    juce::String getAudioStatus() const;
    bool loadAudioFile (const juce::File& file);
    void clearAudioFile();

private:
    struct LoadedAudioFile
    {
        juce::AudioBuffer<float> samples;
        double sampleRate = 44100.0;
        juce::String path;
    };

    struct LoadedModels
    {
        std::shared_ptr<NeuralAudio::NeuralModel> left;
        std::shared_ptr<NeuralAudio::NeuralModel> right;
    };

    std::atomic<float> inputMeterDb {-60.0f};
    std::atomic<float> outputMeterDb {-60.0f};
    juce::String namPath;
    juce::String namStatus {"No NAM model selected"};
    juce::String audioPath;
    juce::String audioStatus {"No audio file loaded; using DAW input"};
    std::shared_ptr<LoadedAudioFile> loadedAudio;
    std::atomic<double> audioPlaybackPosition { 0.0 };
    double currentSampleRate = 44100.0;
    std::shared_ptr<LoadedModels> models;
    juce::AudioBuffer<float> modelScratch { 1, 8192 };
    std::unique_ptr<NeuralAudio::NeuralModelLoader> modelLoader;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMisterNAMAudioProcessor)
};
