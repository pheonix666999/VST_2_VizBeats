#include <JuceHeader.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"

namespace
{
class MainWindow final : public juce::DocumentWindow
{
public:
  explicit MainWindow(VizBeatsAudioProcessor& processor)
      : juce::DocumentWindow("VizBeats", juce::Colours::black, juce::DocumentWindow::allButtons),
        audioProcessor(processor)
  {
    setUsingNativeTitleBar(true);
    setResizable(true, true);

    // Audio hookup: auto-select default output so the click is audible.
    deviceManager.initialiseWithDefaultDevices(0, 2);
    player.setProcessor(&audioProcessor);
    deviceManager.addAudioCallback(&player);

    setContentOwned(audioProcessor.createEditor(), true);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
  }

  void closeButtonPressed() override
  {
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
  }

private:
  VizBeatsAudioProcessor& audioProcessor;
  juce::AudioDeviceManager deviceManager;
  juce::AudioProcessorPlayer player;
};
} // namespace

class VizBeatsStandaloneApplication final : public juce::JUCEApplication
{
public:
  const juce::String getApplicationName() override { return "VizBeats"; }
  const juce::String getApplicationVersion() override { return "1.0.0"; }
  bool moreThanOneInstanceAllowed() override { return true; }

  void initialise(const juce::String&) override
  {
    processor = std::make_unique<VizBeatsAudioProcessor>();
    mainWindow = std::make_unique<MainWindow>(*processor);
  }

  void shutdown() override
  {
    mainWindow = nullptr;
    processor = nullptr;
  }

  void systemRequestedQuit() override
  {
    quit();
  }

  void anotherInstanceStarted(const juce::String&) override
  {
  }

private:
  std::unique_ptr<VizBeatsAudioProcessor> processor;
  std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(VizBeatsStandaloneApplication)

