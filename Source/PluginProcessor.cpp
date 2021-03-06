/*
  ==============================================================================

    This file was auto-generated by the Introjucer!

    It contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace teragon;

//==============================================================================
AtmosfearAudioProcessor::AtmosfearAudioProcessor() :
        voiceController(),
        fileWatcherThread(parameters)
{
    for (size_t i = 0; i < kMaxVoiceCount; ++i) {
        // TODO: Refactor parameter name strings to Constants.h
        parameters.add(new BooleanParameter(getVoiceParamName("Enabled", i), true));
        parameters.add(new StringParameter(getVoiceParamName("Keyword", i), ""));
        parameters.add(new BooleanParameter(getVoiceParamName("Ready", i), false));
        parameters.add(new BooleanParameter(getVoiceParamName("Triggered", i), false));
        parameters.add(new BooleanParameter(getVoiceParamName("Playing", i), false));

        parameters.add(new FloatParameter(getVoiceParamName("Period", i),
                                          kMinPeriodInSec,
                                          kMaxPeriodInSec,
                                          kDefaultPeriodInSec));
        parameters.add(new FloatParameter(getVoiceParamName("Cooldown", i),
                                          kMinPeriodInSec,
                                          kMaxPeriodInSec,
                                          kDefaultPeriodInSec));
        parameters.add(new IntegerParameter(getVoiceParamName("Randomness", i), 0, 100, kDefaultRandomness));

        parameters.add(new DecibelParameter(getVoiceParamName("Volume", i),
                                            kMinVoiceVolume,
                                            kMaxVoiceVolume,
                                            kDefaultVoiceVolume));

        // -1.0 --> 100% left channel
        // +1.0 --> 100% right channel
        //  0.0 --> 100% both channels
        parameters.add(new FloatParameter(getVoiceParamName("Panning", i), -1.0, 1.0, 0.0));
    }

    parameters.add(new IntegerParameter("Selected Voice", 0, kMaxVoiceCount, kMaxVoiceCount));
    parameters.add(new BooleanParameter("Files Added", false));

    randomizeParameters();

    voiceController = new VoiceController(parameters, getSampleRate());
}

void AtmosfearAudioProcessor::randomizeParameters() {
    // Randomize panning/volume a bit for each voice to mix things up a bit
    for (size_t i = 0; i < kMaxVoiceCount; ++i) {
        Parameter *volume = parameters.get(getVoiceParamName("Volume", i));
        randomizeParameter(volume);
        Parameter *panning = parameters.get(getVoiceParamName("Panning", i));
        randomizeParameter(panning);
    }
}

void AtmosfearAudioProcessor::randomizeParameter(Parameter *parameter) {
    const double currentValue = parameter->getValue();
    const bool positive = randomGenerator.nextBool();
    const double percentage = ((double) randomGenerator.nextInt(kMaxRandomizePercentage)) / 100.0;
    const double range = parameter->getMaxValue() - parameter->getMinValue();
    double newValue = currentValue +
                      ((range * percentage) *
                       (positive ? 1.0 : -1.0));

    if (newValue < parameter->getMinValue()) {
        newValue = parameter->getMinValue();
    } else if (newValue > parameter->getMaxValue()) {
        newValue = parameter->getMaxValue();
    }

    printf("Randomizing parameter %s: old value %f, new value %f\n",
           parameter->getName().c_str(), parameter->getValue(), newValue);
    parameters.set(parameter, newValue);
}

//==============================================================================
const String AtmosfearAudioProcessor::getName() const
{
    return "Atmosfear";
}

int AtmosfearAudioProcessor::getNumParameters()
{
    return (int) parameters.size();
}

float AtmosfearAudioProcessor::getParameter (int index)
{
    return (float) parameters.get(index)->getValue();
}

void AtmosfearAudioProcessor::setParameter (int index, float newValue)
{
    parameters.set(index, newValue, nullptr);
}

const String AtmosfearAudioProcessor::getParameterName (int index)
{
    return parameters.get(index)->getName();
}

const String AtmosfearAudioProcessor::getParameterText (int index)
{
    return parameters.get(index)->getDisplayText();
}

const String AtmosfearAudioProcessor::getInputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

const String AtmosfearAudioProcessor::getOutputChannelName (int channelIndex) const
{
    return String (channelIndex + 1);
}

bool AtmosfearAudioProcessor::isInputChannelStereoPair (int /*index*/) const
{
    return true;
}

bool AtmosfearAudioProcessor::isOutputChannelStereoPair (int /*index*/) const
{
    return true;
}

bool AtmosfearAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AtmosfearAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AtmosfearAudioProcessor::silenceInProducesSilenceOut() const
{
    return false;
}

double AtmosfearAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AtmosfearAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AtmosfearAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AtmosfearAudioProcessor::setCurrentProgram (int index)
{
}

const String AtmosfearAudioProcessor::getProgramName (int index)
{
    return String();
}

void AtmosfearAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================
void AtmosfearAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fileWatcherThread.startThread();
}

void AtmosfearAudioProcessor::releaseResources()
{
    fileWatcherThread.signalThreadShouldExit();
    fileWatcherThread.stopThread(1000);
}

void AtmosfearAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    parameters.processRealtimeEvents();

    Parameter *filesAdded = parameters.get("Files Added");
    if (filesAdded->getValue() && fileWatcherThread.voiceBufferIsReady()) {
        voiceController->filesAdded(fileWatcherThread.getVoiceBuffers());
        parameters.set(filesAdded, false);
    }

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // I've added this to avoid people getting screaming feedback
    // when they first compile the plugin, but obviously you don't need to
    // this code if your algorithm already fills all the output channels.
    for (int i = getNumInputChannels(); i < getNumOutputChannels(); ++i) {
        buffer.clear (i, 0, buffer.getNumSamples());
    }

    voiceController->process(buffer);
}

//==============================================================================
bool AtmosfearAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* AtmosfearAudioProcessor::createEditor()
{
    return new AtmosfearAudioProcessorEditor (this, parameters, Resources::getCache());
}

//==============================================================================
void AtmosfearAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AtmosfearAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtmosfearAudioProcessor();
}
