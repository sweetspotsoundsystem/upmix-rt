#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace audio_plugin {

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit AudioPluginAudioProcessorEditor(AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AudioPluginAudioProcessor& processorRef_;

    juce::ComboBox layoutSelector_;
    juce::Slider dryWetSlider_;
    juce::Slider gainSlider_;
    juce::Label layoutLabel_;
    juce::Label dryWetLabel_;
    juce::Label gainLabel_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> layoutAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};

}  // namespace audio_plugin
