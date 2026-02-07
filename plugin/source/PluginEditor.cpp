#include <UpmixRT/PluginEditor.h>

namespace audio_plugin {

AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    AudioPluginAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef_(p) {
    setSize(300, 240);

    // Layout selector
    layoutLabel_.setText("Layout", juce::dontSendNotification);
    addAndMakeVisible(layoutLabel_);

    layoutSelector_.addItemList(
        {"Stereo", "5.1", "7.1.4", "9.1.6", "22.2", "AmbiX"}, 1);
    addAndMakeVisible(layoutSelector_);
    layoutAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef_.getAPVTS(), ParamID::kLayout, layoutSelector_);

    // Dry/Wet slider
    dryWetLabel_.setText("Dry/Wet", juce::dontSendNotification);
    addAndMakeVisible(dryWetLabel_);

    dryWetSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(dryWetSlider_);
    dryWetAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef_.getAPVTS(), ParamID::kDryWet, dryWetSlider_);

    // Gain slider
    gainLabel_.setText("Gain", juce::dontSendNotification);
    addAndMakeVisible(gainLabel_);

    gainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    gainSlider_.setTextValueSuffix(" dB");
    addAndMakeVisible(gainSlider_);
    gainAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef_.getAPVTS(), ParamID::kGain, gainSlider_);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() = default;

void AudioPluginAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("UpmixRT", getLocalBounds().removeFromTop(40),
               juce::Justification::centred);
}

void AudioPluginAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40);  // title space

    auto row1 = area.removeFromTop(30);
    layoutLabel_.setBounds(row1.removeFromLeft(60));
    layoutSelector_.setBounds(row1);

    area.removeFromTop(10);

    auto row2 = area.removeFromTop(30);
    dryWetLabel_.setBounds(row2.removeFromLeft(60));
    dryWetSlider_.setBounds(row2);

    area.removeFromTop(10);

    auto row3 = area.removeFromTop(30);
    gainLabel_.setBounds(row3.removeFromLeft(60));
    gainSlider_.setBounds(row3);
}

}  // namespace audio_plugin
