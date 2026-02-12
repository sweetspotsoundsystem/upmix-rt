#include <UpmixRT/PluginProcessor.h>
#include <UpmixRT/PluginEditor.h>

namespace audio_plugin {

AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(createBusesProperties()),
      apvts_(*this, nullptr, "Parameters", createParameterLayout()) {
    layoutParam_ = apvts_.getRawParameterValue(ParamID::kLayout);
    dryWetParam_ = apvts_.getRawParameterValue(ParamID::kDryWet);
    gainParam_ = apvts_.getRawParameterValue(ParamID::kGain);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor() = default;

juce::AudioProcessor::BusesProperties AudioPluginAudioProcessor::createBusesProperties() {
    // Multi-out: 1 main stereo bus (dry) + 12 aux stereo buses (wet) = 26 channels.
    // This allows full 22.2 wet routing (24 ch) while keeping main 1-2 dry.
    // DAWs like Ableton expose each bus as a routable output pair.
    constexpr int kNumAuxBuses = 12;
    const juce::String auxNames[] = {
        "1/2",  "3/4",  "5/6",  "7/8",
        "9/10", "11/12", "13/14", "15/16",
        "17/18", "19/20", "21/22", "23/24"
    };

    auto props = BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Main", juce::AudioChannelSet::stereo(), true);

    for (int i = 0; i < kNumAuxBuses; ++i)
        props = props.withOutput(auxNames[i], juce::AudioChannelSet::stereo(), false);

    return props;
}

juce::AudioProcessorValueTreeState::ParameterLayout
AudioPluginAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ParamID::kLayout, 1},
        "Layout",
        juce::StringArray{"Stereo", "5.1", "7.1.4", "9.1.6", "22.2", "AmbiX"},
        1  // default: 5.1
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamID::kDryWet, 1},
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        1.0f  // default: fully wet
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamID::kGain, 1},
        "Gain",
        juce::NormalisableRange<float>(-42.0f, 0.0f, 0.1f),
        0.0f  // default: unity (0 dB)
    ));

    return layout;
}

void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    auto layout = static_cast<SpeakerLayout>(static_cast<int>(layoutParam_->load()));

    spatialAnalyzer_.prepare(sampleRate);
    encoder_.prepare(sampleRate);
    decoder_.prepare(sampleRate, layout);
    outputWriter_.prepare(sampleRate);
}

void AudioPluginAudioProcessor::releaseResources() {
    spatialAnalyzer_.reset();
    encoder_.reset();
    decoder_.reset();
    outputWriter_.reset();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    // Input must be stereo
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main output must be stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Each aux output bus must be either stereo or disabled
    for (int i = 1; i < layouts.outputBuses.size(); ++i) {
        const auto& bus = layouts.outputBuses[i];
        if (!bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels beyond input
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    auto layout = static_cast<SpeakerLayout>(static_cast<int>(layoutParam_->load()));
    float dryWetTarget = dryWetParam_->load();
    float gainDbTarget = gainParam_->load();

    int numSamples = buffer.getNumSamples();
    int numOutputChannels = std::min(totalNumOutputChannels, kMaxOutputChannels);

    // Get output write pointers
    float* outputPtrs[kMaxOutputChannels];
    for (int ch = 0; ch < numOutputChannels; ++ch)
        outputPtrs[ch] = buffer.getWritePointer(ch);

    float speakerOutputs[kMaxOutputChannels];
    float bFormat[kNumAmbiChannels];

    // Read input (always stereo)
    const float* inL = buffer.getReadPointer(0);
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer(1) : inL;

    for (int s = 0; s < numSamples; ++s) {
        float L = inL[s];
        float R = inR[s];

        // 1. Spatial analysis
        SpatialParams params = spatialAnalyzer_.process(L, R);

        // 2. B-format encoding (phaseless W/Y + enriched X/Z)
        encoder_.encode(L, R, params, bFormat);

        // 3. Decode to speaker feeds
        decoder_.decode(bFormat, layout, speakerOutputs);

        // 4. Main out stays dry; upmix wet signal is routed to aux outputs
        outputWriter_.writeSample(speakerOutputs, L, R, dryWetTarget,
                                  gainDbTarget, numOutputChannels, outputPtrs, s);
    }
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor() {
    return new AudioPluginAudioProcessorEditor(*this);
}

bool AudioPluginAudioProcessor::hasEditor() const { return true; }
const juce::String AudioPluginAudioProcessor::getName() const { return "UpmixRT"; }
bool AudioPluginAudioProcessor::acceptsMidi() const { return false; }
bool AudioPluginAudioProcessor::producesMidi() const { return false; }
bool AudioPluginAudioProcessor::isMidiEffect() const { return false; }
double AudioPluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AudioPluginAudioProcessor::getNumPrograms() { return 1; }
int AudioPluginAudioProcessor::getCurrentProgram() { return 0; }
void AudioPluginAudioProcessor::setCurrentProgram(int /*index*/) {}
const juce::String AudioPluginAudioProcessor::getProgramName(int /*index*/) { return {}; }
void AudioPluginAudioProcessor::changeProgramName(int /*index*/, const juce::String& /*newName*/) {}

void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
}

}  // namespace audio_plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new audio_plugin::AudioPluginAudioProcessor();
}
