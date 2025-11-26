#include <wx/wx.h>
#include <wx/tglbtn.h>
#include <RtAudio.h>
#include "AudioFile.h"
#include "reverb.h"
#include <vector>
#include <memory>

class AudioStream {
public:
    AudioStream() : dac(std::make_unique<RtAudio>()) {}

    bool start(const std::string& filePath) {
        if (!audioFile.load(filePath)) {
            return false;
        }

        reverb = std::make_unique<Reverb>(audioFile.getSampleRate());

        if (dac->getDeviceCount() < 1) {
            wxLogMessage("No audio devices found!");
            return false;
        }

        RtAudio::StreamParameters parameters;
        parameters.deviceId = dac->getDefaultOutputDevice();
        parameters.nChannels = 2;
        parameters.firstChannel = 0;
        unsigned int sampleRate = audioFile.getSampleRate();
        bufferFrames = 256;

        try {
            dac->openStream(&parameters, NULL, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &AudioStream::callback, this);
            leftBuffer.resize(bufferFrames);
            rightBuffer.resize(bufferFrames);
            dac->startStream();
        } catch (RtAudioError& e) {
            wxLogError(wxString(e.getMessage()));
            return false;
        }

        position = 0;
        return true;
    }

    void stop() {
        if (dac->isStreamOpen()) {
            dac->closeStream();
        }
    }

    void toggleReverb(bool on) {
        if(reverb) reverb->set_enabled(on);
    }

    void toggleFreeze(bool on) {
        if(reverb) reverb->set_freeze(on);
    }

private:
    static int callback(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                        double streamTime, RtAudioStreamStatus status, void *userData) {
        AudioStream* self = static_cast<AudioStream*>(userData);
        float *out = static_cast<float*>(outputBuffer);

        for (unsigned int i = 0; i < nBufferFrames; i++) {
            if (self->position >= self->audioFile.getNumSamplesPerChannel()) {
                self->position = 0; // Loop for now
            }

            if (self->audioFile.getNumChannels() == 1) {
                self->leftBuffer[i] = self->audioFile.samples[0][self->position];
                self->rightBuffer[i] = self->audioFile.samples[0][self->position];
            } else {
                self->leftBuffer[i] = self->audioFile.samples[0][self->position];
                self->rightBuffer[i] = self->audioFile.samples[1][self->position];
            }

            self->position++;
        }

        if(self->reverb) {
            self->reverb->process(self->leftBuffer.data(), self->rightBuffer.data(), nBufferFrames);
        }

        for (unsigned int i = 0; i < nBufferFrames; i++) {
            *out++ = self->leftBuffer[i];
            *out++ = self->rightBuffer[i];
        }

        return 0;
    }

    std::unique_ptr<RtAudio> dac;
    AudioFile<float> audioFile;
    size_t position = 0;
    std::unique_ptr<Reverb> reverb;
    unsigned int bufferFrames;
    std::vector<float> leftBuffer;
    std::vector<float> rightBuffer;
};


class ReverbFrame : public wxFrame {
public:
    ReverbFrame() : wxFrame(NULL, wxID_ANY, "Reverb GUI") {
        wxPanel *panel = new wxPanel(this, wxID_ANY);

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

        wxButton *loadButton = new wxButton(panel, wxID_ANY, "Load WAV");
        toggleButton = new wxToggleButton(panel, wxID_ANY, "Reverb");
        freezeButton = new wxToggleButton(panel, wxID_ANY, "Freeze");

        sizer->Add(loadButton, 0, wxALL | wxEXPAND, 5);
        sizer->Add(toggleButton, 0, wxALL | wxEXPAND, 5);
        sizer->Add(freezeButton, 0, wxALL | wxEXPAND, 5);

        panel->SetSizerAndFit(sizer);

        loadButton->Bind(wxEVT_BUTTON, &ReverbFrame::OnLoad, this);
        toggleButton->Bind(wxEVT_TOGGLEBUTTON, &ReverbFrame::OnToggle, this);
        freezeButton->Bind(wxEVT_TOGGLEBUTTON, &ReverbFrame::OnFreeze, this);

        Bind(wxEVT_CLOSE_WINDOW, &ReverbFrame::OnClose, this);
    }

private:
    void OnLoad(wxCommandEvent& event) {
        wxFileDialog openFileDialog(this, _("Open WAV file"), "", "",
                                   "WAV files (*.wav)|*.wav", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
        if (openFileDialog.ShowModal() == wxID_CANCEL)
            return;

        stream.stop();
        if(!stream.start(openFileDialog.GetPath().ToStdString())) {
            wxLogError("Failed to start audio stream.");
        }
    }

    void OnToggle(wxCommandEvent& event) {
        stream.toggleReverb(event.IsChecked());
    }

    void OnFreeze(wxCommandEvent& event) {
        stream.toggleFreeze(event.IsChecked());
    }

    void OnClose(wxCloseEvent& event) {
        stream.stop();
        Destroy();
    }

    AudioStream stream;
    wxToggleButton *toggleButton;
    wxToggleButton *freezeButton;
};

class ReverbApp : public wxApp {
public:
    virtual bool OnInit() {
        ReverbFrame *frame = new ReverbFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(ReverbApp);
