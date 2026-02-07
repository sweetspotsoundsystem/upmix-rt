#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Constants.h"
#include "SpatialAnalyzer.h"
#include "AmbisonicEncoder.h"
#include "AmbisonicDecoder.h"
#include "OutputWriter.h"

namespace audio_plugin {

class AudioPluginAudioProcessor : public juce::AudioProcessor {
public:
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static BusesProperties createBusesProperties();

    juce::AudioProcessorValueTreeState apvts_;

    SpatialAnalyzer spatialAnalyzer_;
    AmbisonicEncoder encoder_;
    AmbisonicDecoder decoder_;
    OutputWriter outputWriter_;

    std::atomic<float>* layoutParam_ = nullptr;
    std::atomic<float>* dryWetParam_ = nullptr;
    std::atomic<float>* gainParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};

}  // namespace audio_plugin
