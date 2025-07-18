// Copyright (c) Charles J. Cliffe
// SPDX-License-Identifier: GPL-2.0+

#include "TuningCanvas.h"

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#if !wxUSE_GLCANVAS
#error "OpenGL required: set wxUSE_GLCANVAS to 1 and rebuild the library"
#endif

#include "CubicSDR.h"
#include "CubicSDRDefs.h"
#include "AppFrame.h"
#include <algorithm>

wxBEGIN_EVENT_TABLE(TuningCanvas, wxGLCanvas) EVT_PAINT(TuningCanvas::OnPaint)
EVT_IDLE(TuningCanvas::OnIdle)
EVT_MOTION(TuningCanvas::OnMouseMoved)
EVT_LEFT_DOWN(TuningCanvas::OnMouseDown)
EVT_LEFT_UP(TuningCanvas::OnMouseReleased)
EVT_RIGHT_DOWN(TuningCanvas::OnMouseRightDown)
EVT_RIGHT_UP(TuningCanvas::OnMouseRightReleased)

EVT_LEAVE_WINDOW(TuningCanvas::OnMouseLeftWindow)
EVT_ENTER_WINDOW(TuningCanvas::OnMouseEnterWindow)
EVT_MOUSEWHEEL(TuningCanvas::OnMouseWheelMoved)
//EVT_KEY_DOWN(TuningCanvas::OnKeyDown)
//EVT_KEY_UP(TuningCanvas::OnKeyUp)
wxEND_EVENT_TABLE()

TuningCanvas::TuningCanvas(wxWindow *parent, const wxGLAttributes& dispAttrs) :
        InteractiveCanvas(parent, dispAttrs), dragAccum(0), uxDown(0), top(false), bottom(false), freq(-1), bw(-1), center(-1), halfBand(false) {

    glContext = new TuningContext(this, &wxGetApp().GetContext(this), wxGetApp().GetContextAttributes());

    hoverIndex = 0;
    downIndex = 0;
    hoverState = TUNING_HOVER_NONE;
    downState = TUNING_HOVER_NONE;
    dragging = false;

    freqDP = -1.0;
    freqW = (1.0f / 3.0f) * 2.0f;

    bwDP = -1.0 + (2.25 / 3.0);
    bwW = (1.0 / 4.0) * 2.0;

    centerDP = -1.0f + (2.0f / 3.0f) * 2.0f;
    centerW = (1.0f / 3.0f) * 2.0f;

    currentPPM = lastPPM = 0;
}

TuningCanvas::~TuningCanvas() = default;

bool TuningCanvas::changed() {

    auto activeDemod = wxGetApp().getDemodMgr().getCurrentModem();
    
    long long current_freq = 0;
    if (activeDemod != nullptr) {
        freq = activeDemod->getFrequency();
    }
    long long current_bw = wxGetApp().getDemodMgr().getLastBandwidth();
    long long current_center = wxGetApp().getFrequency();
    
    if (current_freq != freq || current_bw != bw || current_center != center) {
        return true;
    }
    
    return false;
}

void TuningCanvas::setHalfBand(bool hb) {
    halfBand = hb;
	Refresh();
}

void TuningCanvas::OnPaint(wxPaintEvent& WXUNUSED(event)) {
 //   wxPaintDC dc(this);
    const wxSize ClientSize = GetClientSize() * GetContentScaleFactor();
    
    glContext->SetCurrent(*this);
    initGLExtensions();
    glViewport(0, 0, ClientSize.x, ClientSize.y);

    glContext->DrawBegin();

    auto activeDemod = wxGetApp().getDemodMgr().getCurrentModem();
    
    freq = 0;
    if (activeDemod != nullptr) {
        freq = activeDemod->getFrequency();
    }
    bw = wxGetApp().getDemodMgr().getLastBandwidth();
    center = wxGetApp().getFrequency();

    if (mouseTracker.mouseDown()) {
        glContext->Draw(ThemeMgr::mgr.currentTheme->tuningBarDark.r, ThemeMgr::mgr.currentTheme->tuningBarDark.g, ThemeMgr::mgr.currentTheme->tuningBarDark.b,
                0.75, mouseTracker.getOriginMouseX(), mouseTracker.getMouseX());
    }

    RGBA4f clr = top ? ThemeMgr::mgr.currentTheme->tuningBarUp : ThemeMgr::mgr.currentTheme->tuningBarDown;

    RGBA4f clrDark = ThemeMgr::mgr.currentTheme->tuningBarDark;
    RGBA4f clrMid = ThemeMgr::mgr.currentTheme->tuningBarLight;

    glContext->DrawTunerBarIndexed(1, 3, 11, freqDP, freqW, clrMid, 0.25, true, true); // freq
    glContext->DrawTunerBarIndexed(4, 6, 11, freqDP, freqW, clrDark, 0.25, true, true);
    glContext->DrawTunerBarIndexed(7, 9, 11, freqDP, freqW, clrMid, 0.25, true, true);
    glContext->DrawTunerBarIndexed(10, 11, 11, freqDP, freqW, clrDark, 0.25, true, true);

    glContext->DrawTunerBarIndexed(1, 3, 7, bwDP, bwW, clrMid, 0.25, true, true); // bw
    glContext->DrawTunerBarIndexed(4, 6, 7, bwDP, bwW, clrDark, 0.25, true, true);
    glContext->DrawTunerBarIndexed(7, 7, 7, bwDP, bwW, clrMid, 0.25, true, true);

    glContext->DrawTunerBarIndexed(1, 3, 11, centerDP, centerW, clrMid, 0.25, true, true); // freq
    glContext->DrawTunerBarIndexed(4, 6, 11, centerDP, centerW, clrDark, 0.25, true, true);
    glContext->DrawTunerBarIndexed(7, 9, 11, centerDP, centerW, clrMid, 0.25, true, true);
    glContext->DrawTunerBarIndexed(10, 11, 11, centerDP, centerW, clrDark, 0.25, true, true);

    if (hoverIndex > 0 && !mouseTracker.mouseDown()) {
        switch (hoverState) {

        case TUNING_HOVER_FREQ:
            glContext->DrawTunerBarIndexed(hoverIndex, hoverIndex, 11, freqDP, freqW, clr, 0.25, top, bottom); // freq
            break;
        case TUNING_HOVER_BW:
            glContext->DrawTunerBarIndexed(hoverIndex, hoverIndex, 7, bwDP, bwW, clr, 0.25, top, bottom); // bw
            break;
        case TUNING_HOVER_CENTER:
            glContext->DrawTunerBarIndexed(hoverIndex, hoverIndex, 11, centerDP, centerW, clr, 0.25, top, bottom); // center
            break;
        case TUNING_HOVER_NONE:
            break;
        case TUNING_HOVER_PPM:
            glContext->DrawTunerBarIndexed(hoverIndex, hoverIndex, 11, freqDP, freqW, clr, 0.25, top, bottom); // freq
            break;
         }
    }

    if (altDown) {
        glContext->DrawTuner(currentPPM, 11, freqDP, freqW);
    } else {
        glContext->DrawTuner(freq, 11, freqDP, freqW);
        int snap = wxGetApp().getFrequencySnap();
        if (snap != 1) {
            glContext->DrawTunerDigitBox((int)log10(snap), 11, freqDP, freqW, RGBA4f(1.0,0.0,0.0));
        }
    }
    glContext->DrawTuner(halfBand?(bw/2):bw, 7, bwDP, bwW);
    glContext->DrawTuner(center, 11, centerDP, centerW);

    glContext->DrawEnd();

    SwapBuffers();
}

/***
 * Perform tuner step
 *
 * @param state     Current hover state
 * @param digit     Digit position
 * @param tuningDir        Tuning direction, true for up
 * @param preventCarry Prevent carry operation on digit overflow
 * @param zeroOut   Zero out 'digit' and lower digits
 */
void TuningCanvas::StepTuner(ActiveState state, TuningDirection tuningDir, int digit, bool preventCarry, bool zeroOut) {
    double exp = pow(10, digit);
    long long amount = tuningDir ? exp : -exp;

    if (halfBand && state == TUNING_HOVER_BW) {
        amount *= 2;
    }
    
    auto activeDemod = wxGetApp().getDemodMgr().getCurrentModem();
    if (state == TUNING_HOVER_FREQ && activeDemod) {
        long long demod_freq = activeDemod->getFrequency();
        long long diff = abs(wxGetApp().getFrequency() - demod_freq);

        if (zeroOut) { // Zero digits to right
            double intpart;
            modf(demod_freq / (exp * 10), &intpart);
            demod_freq = intpart * exp * 10;
        } else if (preventCarry) { // Prevent digit from carrying
            bool carried = (long long)((demod_freq) / (exp * 10)) != (long long)((demod_freq + amount) / (exp * 10)) || (bottom && demod_freq < exp);
            demod_freq += carried ? (9 * -amount) : amount;
        } else {
            demod_freq += amount;
        }

        if (wxGetApp().getSampleRate() / 2 < diff) {
            wxGetApp().setFrequency(demod_freq);
            printf("HEY 1: [demod_freq = %lld]\n");
        }

        activeDemod->setTracking(true);
        activeDemod->setFollow(true);
        activeDemod->setFrequency(demod_freq);
        printf("HEY 2: [demod_freq = %lld]\n");
        if (activeDemod->isDeltaLock()) {
            activeDemod->setDeltaLockOfs(activeDemod->getFrequency() - wxGetApp().getFrequency());
        }
        activeDemod->updateLabel(demod_freq);
    }

    if (state == TUNING_HOVER_BW) {
        long nbw = wxGetApp().getDemodMgr().getLastBandwidth();

        if (zeroOut) { // Zero digits to right
            double intpart;
            modf(nbw / (exp * 10), &intpart);
            nbw = intpart * exp * 10;
        } else if (preventCarry) { // Prevent digit from carrying
            bool carried = (long)((nbw) / (exp * 10)) != (long)((nbw + amount) / (exp * 10)) || (bottom && nbw < exp);
            nbw += carried ? (9 * -amount) : amount;
        } else {
            nbw += amount;
        }

        if (nbw > CHANNELIZER_RATE_MAX) {
            nbw = CHANNELIZER_RATE_MAX;
        }

        wxGetApp().getDemodMgr().setLastBandwidth(nbw);

        if (activeDemod) {
            activeDemod->setBandwidth(wxGetApp().getDemodMgr().getLastBandwidth());
        }
    }

    if (state == TUNING_HOVER_CENTER) {
        long long ctr = wxGetApp().getFrequency();
        if (zeroOut) { // Zero digits to right
            double intpart;
            modf(ctr / (exp * 10), &intpart);
            ctr = intpart * exp * 10;
        } else if (preventCarry) { // Prevent digit from carrying
            bool carried = (long long)((ctr) / (exp * 10)) != (long long)((ctr + amount) / (exp * 10)) || (bottom && ctr < exp);
            ctr += carried?(9*-amount):amount;
        } else {
            ctr += amount;
        }

        wxGetApp().setFrequency(ctr);
        printf("HEY 3: [demod_freq = %lld]\n");
    }

    if (state == TUNING_HOVER_PPM) {
        if (preventCarry) {
            bool carried = (long long)((currentPPM) / (exp * 10)) != (long long)((currentPPM + amount) / (exp * 10)) || (bottom && currentPPM < exp);
            currentPPM += carried?(9*-amount):amount;
        } else {
            currentPPM += amount;
        }

        if (currentPPM > 2000) {
            currentPPM = 2000;
        }

        if (currentPPM < -2000) {
            currentPPM = -2000;
        }

        wxGetApp().setPPM(currentPPM);
        wxGetApp().notifyMainUIOfDeviceChange();
    }
}

void TuningCanvas::OnIdle(wxIdleEvent & event) {
    if (mouseTracker.mouseDown()) {
        if (downState != TUNING_HOVER_NONE) {
            dragAccum += 5.0*mouseTracker.getOriginDeltaMouseX();
            while (dragAccum > 1.0) {
                StepTuner(downState, TUNING_DIRECTION_UP, downIndex - 1, shiftDown, false);
                dragAccum -= 1.0;
                dragging = true;
            }
            while (dragAccum < -1.0) {
                StepTuner(downState, TUNING_DIRECTION_DOWN, downIndex - 1, shiftDown, false);
                dragAccum += 1.0;
                dragging = true;
            }
        } else {
            dragAccum = 0;
            dragging = false;
        }
    }
    if (mouseTracker.mouseInView() || changed()) {
        Refresh();
    }
}

void TuningCanvas::OnMouseMoved(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseMoved(event);

    top = mouseTracker.getMouseY() >= 0.5;
    bottom = mouseTracker.getMouseY() <= 0.5;

    int index = glContext->GetTunerDigitIndex(mouseTracker.getMouseX(), 11, freqDP, freqW); // freq
    if (index > 0) {
        hoverIndex = index;
        hoverState = altDown?TUNING_HOVER_PPM:TUNING_HOVER_FREQ;
    }

    if (!index) {
        index = glContext->GetTunerDigitIndex(mouseTracker.getMouseX(), 7, bwDP, bwW); // bw
        if (index > 0) {
            hoverIndex = index;
            hoverState = TUNING_HOVER_BW;
        }
    }

    if (!index) {
        index = glContext->GetTunerDigitIndex(mouseTracker.getMouseX(), 11, centerDP, centerW); // center
        if (index > 0) {
            hoverIndex = index;
            hoverState = TUNING_HOVER_CENTER;
        }
    }

    if (!index) {
        hoverIndex = 0;
        hoverState = TUNING_HOVER_NONE;
    } else {
        switch (hoverState) {
        case TUNING_HOVER_FREQ:
                setStatusText("Click, wheel or drag(left/right) a digit to change frequency; SPACE or numeric key for direct input. Right click to set/clear snap. Hold ALT to change PPM. Hold SHIFT to disable carry. SHIFT-right click to Zero Right.");
            break;
        case TUNING_HOVER_BW:
                setStatusText("Click, wheel or drag(left/right) a digit to change bandwidth; SPACE or numeric key for direct input.  Hold SHIFT to disable carry. SHIFT-right click to Zero Right.");
            break;
        case TUNING_HOVER_CENTER:
                setStatusText("Click, wheel or drag(left/right) a digit to change center frequency; SPACE or numeric key for direct input.  Hold SHIFT to disable carry. SHIFT-right click to Zero Right.");
            break;
        case TUNING_HOVER_PPM:
                 setStatusText("Click, wheel or drag(left/right) a digit to change device PPM offset.  Hold SHIFT to disable carry.");
             break;
        case TUNING_HOVER_NONE:
            setStatusText("");
            break;
      }
    }

    if (hoverState == TUNING_HOVER_BW || hoverState == TUNING_HOVER_FREQ) {
         wxGetApp().getDemodMgr().setActiveDemodulator(wxGetApp().getDemodMgr().getCurrentModem());
     } else {
         wxGetApp().getDemodMgr().setActiveDemodulator(nullptr);
     }
}

void TuningCanvas::OnMouseDown(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseDown(event);

    uxDown = 2.0 * (mouseTracker.getMouseX() - 0.5);
    dragAccum = 0;

    mouseTracker.setVertDragLock(true);
    downIndex = hoverIndex;
    downState = hoverState;
}

void TuningCanvas::OnMouseWheelMoved(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseWheelMoved(event);

    int hExponent = hoverIndex - 1;

    if (hoverState != TUNING_HOVER_NONE && !mouseTracker.mouseDown() && hoverIndex) {
        if (event.m_wheelAxis == wxMOUSE_WHEEL_VERTICAL) {
            StepTuner(hoverState, (event.m_wheelRotation > 0) ? TUNING_DIRECTION_UP : TUNING_DIRECTION_DOWN, hExponent, shiftDown, false);
        } else {
            StepTuner(hoverState, (event.m_wheelRotation < 0) ? TUNING_DIRECTION_UP : TUNING_DIRECTION_DOWN, hExponent, shiftDown, false);
        }
    }
}

void TuningCanvas::OnMouseReleased(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseReleased(event);

    int hExponent = hoverIndex - 1;

    if (hoverState != TUNING_HOVER_NONE && !dragging && (downState == hoverState) && (downIndex == hoverIndex)) {
        StepTuner(hoverState, top ? TUNING_DIRECTION_UP : TUNING_DIRECTION_DOWN, hExponent, shiftDown, false);
    }

    mouseTracker.setVertDragLock(false);

    dragging = false;
    SetCursor(wxCURSOR_ARROW);
}

void TuningCanvas::OnMouseRightDown(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseRightDown(event);

    uxDown = 2.0 * (mouseTracker.getMouseX() - 0.5);

    downIndex = hoverIndex;
    downState = hoverState;
}

void TuningCanvas::OnMouseRightReleased(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseRightReleased(event);

    if (shiftDown) {
        int hExponent = hoverIndex - 1;

        if (hoverState != TUNING_HOVER_NONE && !dragging && (downState == hoverState) && (downIndex == hoverIndex)) {
            StepTuner(hoverState, top ? TUNING_DIRECTION_UP : TUNING_DIRECTION_DOWN, hExponent, false, true);
        }
    } else if (hoverState == TUNING_HOVER_FREQ) {
        if (hoverIndex == 1) {
            wxGetApp().setFrequencySnap(1);
            printf("HEY 4: [snap = 1]\n");
        } else if (hoverIndex > 1 && hoverIndex < 8) {
            int exp = pow(10, hoverIndex-1);
            if (wxGetApp().getFrequencySnap() == exp) {
                wxGetApp().setFrequencySnap(1);
                printf("HEY 5: [snap = 1]\n");
            } else {
                wxGetApp().setFrequencySnap(exp);
                printf("HEY 6: [snap = %d]\n", exp);
            }
        }
    }
}

void TuningCanvas::OnMouseLeftWindow(wxMouseEvent& event) {
    InteractiveCanvas::OnMouseLeftWindow(event);
    SetCursor(wxCURSOR_CROSS);
    hoverIndex = 0;
    hoverState = TUNING_HOVER_NONE;
    wxGetApp().getDemodMgr().setActiveDemodulator(nullptr);

    if (currentPPM != lastPPM) {
        wxGetApp().saveConfig();
    }
    Refresh();
}

void TuningCanvas::OnMouseEnterWindow(wxMouseEvent& event) {
    InteractiveCanvas::mouseTracker.OnMouseEnterWindow(event);
    SetCursor(wxCURSOR_ARROW);
    hoverIndex = 0;
    hoverState = TUNING_HOVER_NONE;
    lastPPM = currentPPM = wxGetApp().getPPM();
#ifdef _WIN32
	if (wxGetApp().getAppFrame()->canFocus()) {
		this->SetFocus();
	}
#endif
}

void TuningCanvas::setHelpTip(std::string tip) {
    helpTip = tip;
}

void TuningCanvas::OnKeyDown(wxKeyEvent& event) {
    InteractiveCanvas::OnKeyDown(event);

    if (event.GetKeyCode() == WXK_SPACE) {
        if (hoverState == TUNING_HOVER_CENTER || hoverState == TUNING_HOVER_FREQ) {
            wxGetApp().showFrequencyInput(FrequencyDialog::FDIALOG_TARGET_DEFAULT);
        } else if (hoverState == TUNING_HOVER_BW) {
            wxGetApp().showFrequencyInput(FrequencyDialog::FDIALOG_TARGET_BANDWIDTH);
        }
    } 
}

void TuningCanvas::OnKeyUp(wxKeyEvent& event) {
    InteractiveCanvas::OnKeyUp(event);
}

TuningCanvas::ActiveState TuningCanvas::getHoverState() {
    return hoverState;
}
