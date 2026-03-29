#include "RenderDialog.h"
#include "../PluginProcessor.h"
#include "../Common/ArrangementTimeline.h"
#include "../Common/GlobalTransport.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace axelf::ui
{

// ─────────────────────────────────────────────────────────────
//  RenderDialog implementation
// ─────────────────────────────────────────────────────────────

void RenderDialog::reset()
{
    state = State::Idle;
    progressValue = 0.0;
    progressBar->setVisible(false);
    statusLabel.setVisible(false);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
    errorLabel.setVisible(false);
    cancelButton.setButtonText("Cancel");
    renderButton.setButtonText("Render  \xe2\x96\xb6");
    renderButton.setEnabled(true);

    // Auto-select sample rate to match current host rate
    double sr = processor.getSampleRate();
    if (sr >= 88000.0 && sr < 92000.0)
        rateCombo.setSelectedId(3, juce::dontSendNotification);
    else if (sr >= 92000.0)
        rateCombo.setSelectedId(4, juce::dontSendNotification);
    else if (sr >= 47000.0)
        rateCombo.setSelectedId(2, juce::dontSendNotification);
    else
        rateCombo.setSelectedId(1, juce::dontSendNotification);

    // Default output path = sessions dir / session name
    if (chosenPath == juce::File())
    {
        auto sessionFile = processor.getCurrentSessionFile();
        auto dir = sessionFile.existsAsFile()
                       ? sessionFile.getParentDirectory()
                       : AxelFProcessor::getSessionsDirectory();
        auto name = sessionFile.existsAsFile()
                        ? sessionFile.getFileNameWithoutExtension()
                        : "Untitled";
        chosenPath = dir.getChildFile(juce::File::createLegalFileName(name) + ".wav");
    }

    updateSummary();
    updatePathLabel();
}

void RenderDialog::updateSummary()
{
    auto& arr = processor.getArrangement();
    auto& t   = processor.getTransport();
    int bpb   = t.getTimeSignatureNumerator();
    int bars  = arr.getTotalBars(bpb);

    static constexpr int kTailValues[] = { 0, 1, 2, 4 };
    int tailIdx = tailCombo.getSelectedId() - 1;
    int tail = (tailIdx >= 0 && tailIdx < 4) ? kTailValues[tailIdx] : 0;

    double bpm     = static_cast<double>(t.getBpm());
    double beats   = static_cast<double>(bars + tail) * bpb;
    double seconds = (bpm > 0.0) ? (beats / (bpm / 60.0)) : 0.0;

    juce::String text = juce::String(bars) + " bars";
    if (tail > 0)
        text += " + " + juce::String(tail) + " tail";
    text += juce::String::charToString(0x2009) + juce::String::charToString(0x2014) + juce::String::charToString(0x2009);
    text += juce::String(seconds, 1) + " sec";

    if (tracksButton.getToggleState())
        text += "  " + juce::String::charToString(0x2192) + "  7 files";

    summaryLabel.setText(text, juce::dontSendNotification);
}

void RenderDialog::updatePathLabel()
{
    if (chosenPath == juce::File())
    {
        pathLabel.setText("(no path selected)", juce::dontSendNotification);
        return;
    }

    if (tracksButton.getToggleState())
        pathLabel.setText(chosenPath.getParentDirectory().getFullPathName(),
                          juce::dontSendNotification);
    else
        pathLabel.setText(chosenPath.getFullPathName(), juce::dontSendNotification);
}

void RenderDialog::browseForPath()
{
    if (state == State::Rendering) return;

    if (tracksButton.getToggleState())
    {
        // Folder picker for individual tracks
        fileChooser = std::make_unique<juce::FileChooser>(
            "Choose output folder",
            chosenPath.existsAsFile() ? chosenPath.getParentDirectory()
                                      : AxelFProcessor::getSessionsDirectory());

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result != juce::File())
                {
                    chosenPath = result;
                    updatePathLabel();
                }
            });
    }
    else
    {
        // File picker for full mix
        fileChooser = std::make_unique<juce::FileChooser>(
            "Save rendered file",
            chosenPath.existsAsFile() ? chosenPath : AxelFProcessor::getSessionsDirectory(),
            "*.wav;*.aiff");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result != juce::File())
                {
                    chosenPath = result;
                    updatePathLabel();
                }
            });
    }
}

void RenderDialog::onCancel()
{
    if (state == State::Rendering)
    {
        // Abort the render
        if (renderThread != nullptr)
        {
            renderThread->signalThreadShouldExit();
            renderThread->waitForThreadToExit(5000);
            renderThread.reset();
        }
        // Clean up partial files
        if (chosenPath.existsAsFile())
            chosenPath.deleteFile();

        state = State::Idle;
        progressBar->setVisible(false);
        statusLabel.setVisible(false);
        errorLabel.setVisible(false);
        cancelButton.setButtonText("Cancel");
        renderButton.setButtonText("Render  \xe2\x96\xb6");
        renderButton.setEnabled(true);
        repaint();
    }
    else
    {
        dismiss();
    }
}

void RenderDialog::onRender()
{
    errorLabel.setVisible(false);

    // Validate: has arrangement content?
    auto& arr = processor.getArrangement();
    int bpb   = processor.getTransport().getTimeSignatureNumerator();
    if (arr.getMode() != ArrangementMode::Song || arr.getTotalBars(bpb) <= 0)
    {
        errorLabel.setText("Switch to SONG mode and add blocks to render.",
                           juce::dontSendNotification);
        errorLabel.setVisible(true);
        return;
    }

    // Validate: path chosen?
    if (chosenPath == juce::File())
    {
        errorLabel.setText("Choose an output path first.", juce::dontSendNotification);
        errorLabel.setVisible(true);
        return;
    }

    startRender();
}

void RenderDialog::startRender()
{
    state = State::Rendering;
    progressValue = 0.0;
    progressBar->setVisible(true);
    statusLabel.setText("Rendering... 0%", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(Colours::textPrimary));
    statusLabel.setVisible(true);
    errorLabel.setVisible(false);

    cancelButton.setButtonText("Abort");
    renderButton.setButtonText("Rendering...");
    renderButton.setEnabled(false);

    // Build options
    OfflineRenderThread::Options opts;
    opts.individualTracks = tracksButton.getToggleState();
    opts.normalize = normalizeToggle.getToggleState();

    switch (formatCombo.getSelectedId())
    {
        case 1: opts.bitDepth = 16; opts.floatFormat = false; opts.useAiff = false; break;
        case 2: opts.bitDepth = 24; opts.floatFormat = false; opts.useAiff = false; break;
        case 3: opts.bitDepth = 32; opts.floatFormat = true;  opts.useAiff = false; break;
        case 4: opts.bitDepth = 24; opts.floatFormat = false; opts.useAiff = true;  break;
        default: opts.bitDepth = 24; break;
    }

    static constexpr double kRates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
    int rateIdx = rateCombo.getSelectedId() - 1;
    opts.sampleRate = (rateIdx >= 0 && rateIdx < 4) ? kRates[rateIdx] : 48000.0;

    static constexpr int kTailValues[] = { 0, 1, 2, 4 };
    int tailIdx = tailCombo.getSelectedId() - 1;
    opts.tailBars = (tailIdx >= 0 && tailIdx < 4) ? kTailValues[tailIdx] : 0;

    auto sessionFile = processor.getCurrentSessionFile();
    opts.baseName = sessionFile.existsAsFile()
                        ? sessionFile.getFileNameWithoutExtension()
                        : "Untitled";
    opts.baseName = juce::File::createLegalFileName(opts.baseName);

    if (opts.individualTracks)
    {
        opts.outputDir = chosenPath.isDirectory() ? chosenPath : chosenPath.getParentDirectory();
    }
    else
    {
        opts.outputFile = chosenPath;
    }

    renderThread = std::make_unique<OfflineRenderThread>(processor, opts);
    renderThread->startThread();
    startTimerHz(15);  // poll progress at 15 fps
}


// ─────────────────────────────────────────────────────────────
//  OfflineRenderThread — the actual render loop
// ─────────────────────────────────────────────────────────────

void OfflineRenderThread::run()
{
    auto& transport   = processor.getTransport();
    auto& arrangement = processor.getArrangement();
    int bpb = transport.getTimeSignatureNumerator();
    double bpm = static_cast<double>(transport.getBpm());

    int totalBars = arrangement.getTotalBars(bpb) + options.tailBars;
    if (totalBars <= 0 || bpm <= 0.0)
    {
        errorMessage = "Nothing to render (no bars or invalid BPM).";
        succeeded.store(false);
        return;
    }

    double totalBeats   = static_cast<double>(totalBars) * bpb;
    double totalSeconds = totalBeats / (bpm / 60.0);
    int64_t totalSamples = static_cast<int64_t>(totalSeconds * options.sampleRate);
    int blockSize = 512;

    // ── 1. Snapshot processor state ──────────────────────
    juce::MemoryBlock stateSnapshot;
    processor.getStateInformation(stateSnapshot);
    double origSampleRate = processor.getSampleRate();

    // ── 2. Suspend audio thread & re-prepare ─────────────
    processor.suspendProcessing(true);
    processor.prepareToPlay(options.sampleRate, blockSize);

    // ── 3. Reset transport to start ──────────────────────
    transport.stop();
    transport.seekToPosition(0.0);
    transport.play();

    // ── 4. Create audio format writers ───────────────────
    std::unique_ptr<juce::AudioFormat> format;
    juce::String extension;
    if (options.useAiff)
    {
        format = std::make_unique<juce::AiffAudioFormat>();
        extension = ".aiff";
    }
    else
    {
        format = std::make_unique<juce::WavAudioFormat>();
        extension = ".wav";
    }

    int bitsPerSample = options.floatFormat ? 32 : options.bitDepth;

    // Helper to create a writer for a file
    auto createWriter = [&](const juce::File& file) -> std::unique_ptr<juce::AudioFormatWriter>
    {
        file.deleteFile();
        auto stream = std::make_unique<juce::FileOutputStream>(file);
        if (stream->failedToOpen())
            return nullptr;

        juce::StringPairArray metadata;
        if (options.floatFormat)
        {
            // WAV 32-bit float
            auto* writer = format->createWriterFor(
                stream.get(), options.sampleRate, 2, 32,
                metadata, 0);
            if (writer != nullptr)
                stream.release();
            return std::unique_ptr<juce::AudioFormatWriter>(writer);
        }
        else
        {
            auto* writer = format->createWriterFor(
                stream.get(), options.sampleRate, 2, bitsPerSample,
                metadata, 0);
            if (writer != nullptr)
                stream.release();
            return std::unique_ptr<juce::AudioFormatWriter>(writer);
        }
    };

    // Main mix writer
    juce::File mixFile;
    if (options.individualTracks)
        mixFile = options.outputDir.getChildFile(options.baseName + "_Mix" + extension);
    else
        mixFile = options.outputFile;

    auto mixWriter = createWriter(mixFile);
    if (mixWriter == nullptr)
    {
        errorMessage = "Failed to create output file: " + mixFile.getFullPathName();
        processor.setStateInformation(stateSnapshot.getData(),
                                      static_cast<int>(stateSnapshot.getSize()));
        processor.prepareToPlay(origSampleRate, blockSize);
        processor.suspendProcessing(false);
        succeeded.store(false);
        return;
    }

    // Per-module stem writers (only if individual tracks)
    static constexpr const char* kModuleNames[] = {
        "Jupiter8", "Moog", "JX3P", "DX7", "PPGWave", "LinnDrum"
    };
    std::array<std::unique_ptr<juce::AudioFormatWriter>, 6> stemWriters;

    if (options.individualTracks)
    {
        for (int i = 0; i < 6; ++i)
        {
            auto file = options.outputDir.getChildFile(
                options.baseName + "_" + kModuleNames[i] + extension);
            stemWriters[static_cast<size_t>(i)] = createWriter(file);
            if (stemWriters[static_cast<size_t>(i)] == nullptr)
            {
                errorMessage = "Failed to create stem file: " + file.getFullPathName();
                processor.setStateInformation(stateSnapshot.getData(),
                                              static_cast<int>(stateSnapshot.getSize()));
                processor.prepareToPlay(origSampleRate, blockSize);
                processor.suspendProcessing(false);
                succeeded.store(false);
                return;
            }
        }
    }

    // ── 5. Render loop ───────────────────────────────────
    juce::AudioBuffer<float> mixBuffer(2, blockSize);
    juce::MidiBuffer midiBuf;
    int64_t samplesWritten = 0;
    float peakSample = 0.0f;

    while (samplesWritten < totalSamples)
    {
        if (threadShouldExit())
        {
            // Clean up partial files on abort
            mixWriter.reset();
            mixFile.deleteFile();
            if (options.individualTracks)
            {
                for (int i = 0; i < 6; ++i)
                {
                    stemWriters[static_cast<size_t>(i)].reset();
                    options.outputDir.getChildFile(
                        options.baseName + "_" + kModuleNames[i] + extension).deleteFile();
                }
            }
            processor.setStateInformation(stateSnapshot.getData(),
                                          static_cast<int>(stateSnapshot.getSize()));
            processor.prepareToPlay(origSampleRate, blockSize);
            processor.suspendProcessing(false);
            succeeded.store(false);
            errorMessage = "Render cancelled.";
            return;
        }

        int samplesThisBlock = static_cast<int>(
            std::min(static_cast<int64_t>(blockSize), totalSamples - samplesWritten));

        mixBuffer.setSize(2, samplesThisBlock, false, false, true);
        mixBuffer.clear();
        midiBuf.clear();

        processor.processBlock(mixBuffer, midiBuf);

        // Write mix
        mixWriter->writeFromAudioSampleBuffer(mixBuffer, 0, samplesThisBlock);

        // Track peak for normalize
        if (options.normalize)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto mag = mixBuffer.getMagnitude(ch, 0, samplesThisBlock);
                if (mag > peakSample) peakSample = mag;
            }
        }

        // Write stems — the per-module buffers are populated inside processBlock.
        // We access them via the processor's public module buffer accessors.
        // NOTE: processBlock populates these as a side-effect.
        // For stem export, we tap the per-module buffers AFTER processBlock
        // has been called. This gives us post-fader, pre-send audio.
        // (The actual module buffers are private members of AxelFProcessor;
        //  stem capture requires a public accessor — see TODO below.)

        samplesWritten += samplesThisBlock;
        progress.store(static_cast<double>(samplesWritten)
                       / static_cast<double>(totalSamples));
    }

    // ── 6. Flush and close ───────────────────────────────
    mixWriter.reset();
    for (auto& w : stemWriters)
        w.reset();

    // ── 7. Normalize if requested ────────────────────────
    if (options.normalize && peakSample > 0.0001f)
    {
        float targetPeak = std::pow(10.0f, -0.1f / 20.0f);  // -0.1 dBFS
        float gain = targetPeak / peakSample;

        auto normalizeFile = [&](const juce::File& file)
        {
            juce::WavAudioFormat wavFmt;
            juce::AiffAudioFormat aiffFmt;
            juce::AudioFormat* readFmt = options.useAiff
                ? static_cast<juce::AudioFormat*>(&aiffFmt)
                : static_cast<juce::AudioFormat*>(&wavFmt);

            auto inStream = file.createInputStream();
            if (inStream == nullptr) return;

            std::unique_ptr<juce::AudioFormatReader> reader(
                readFmt->createReaderFor(inStream.release(), true));
            if (reader == nullptr) return;

            auto tempFile = file.getSiblingFile(file.getFileNameWithoutExtension() + "_norm_tmp" + extension);
            auto outWriter = createWriter(tempFile);
            if (outWriter == nullptr) return;

            juce::AudioBuffer<float> buf(2, 4096);
            int64_t pos = 0;
            while (pos < reader->lengthInSamples)
            {
                int toRead = static_cast<int>(
                    std::min(static_cast<int64_t>(4096), reader->lengthInSamples - pos));
                reader->read(&buf, 0, toRead, pos, true, true);
                buf.applyGain(0, toRead, gain);
                outWriter->writeFromAudioSampleBuffer(buf, 0, toRead);
                pos += toRead;
            }

            outWriter.reset();
            reader.reset();

            tempFile.moveFileTo(file);
        };

        normalizeFile(mixFile);
        // Stem normalization would need per-file peak tracking — deferred for now
    }

    // ── 8. Restore processor state ───────────────────────
    processor.setStateInformation(stateSnapshot.getData(),
                                  static_cast<int>(stateSnapshot.getSize()));
    processor.prepareToPlay(origSampleRate, blockSize);
    processor.suspendProcessing(false);

    succeeded.store(true);
}

} // namespace axelf::ui
