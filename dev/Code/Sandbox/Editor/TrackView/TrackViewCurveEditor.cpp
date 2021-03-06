/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "stdafx.h"
#include "TrackViewCurveEditor.h"

#include "TrackViewDialog.h"
#include "TrackViewTrack.h"
#include "AnimationContext.h"

#include "TrackView/ui_TrackViewCurveEditor.h"

#if defined(Q_OS_WIN)
#include <QtWinExtras/QtWin>
#endif


#define IDC_TRACKVIEWGRAPH_CURVE 1
#define IDC_TIMELINE             2

#define IDC_HORIZON_SLIDER      3
#define IDC_VERTICAL_SLIDER     4

//! It's for mapping from a slider control range to a real zoom range, and vice versa.
#define SLIDER_MULTIPLIER       100.f
#define SLIDERRANGE_TO_ZOOM(SLIDERVALUE)    (float)SLIDERVALUE / SLIDER_MULTIPLIER
#define ZOOMRANGE_TO_SLIDER(ZOOMVALUE)      (int)(ZOOMVALUE * SLIDER_MULTIPLIER)


//////////////////////////////////////////////////////////////////////////
TrackViewCurveEditorDialog::TrackViewCurveEditorDialog(QWidget* parent)
    : QWidget(parent)
{
    m_widget = new CTrackViewCurveEditor(this);
    QVBoxLayout* l = new QVBoxLayout;
    l->setMargin(0);
    l->addWidget(m_widget);
    setLayout(l);
}

//////////////////////////////////////////////////////////////////////////
CTrackViewCurveEditor::CTrackViewCurveEditor(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::TrackViewCurveEditor)
{
    m_ui->setupUi(this);
    m_bLevelClosing = false;
    m_bIgnoreSelfEvents = false;
    GetIEditor()->RegisterNotifyListener(this);
    GetIEditor()->GetAnimation()->AddListener(this);

    m_timelineCtrl.SetTimeRange(Range(0, 1));
    m_timelineCtrl.SetTicksTextScale(1.0f);

    m_ui->m_wndSpline->SetTimelineCtrl(&m_timelineCtrl);

    connect(m_ui->m_horizonSlider, &QAbstractSlider::valueChanged, this, &CTrackViewCurveEditor::OnHorizonSliderChange);
    connect(m_ui->m_verticlSlider, &QAbstractSlider::valueChanged, this, &CTrackViewCurveEditor::OnVerticalSliderChange);
    connect(&m_timelineCtrl, &TimelineWidget::change, this, &CTrackViewCurveEditor::OnTimelineChange);
    connect(m_ui->m_wndSpline, &SplineWidget::scrollZoomRequested, this, &CTrackViewCurveEditor::OnSplineScrollZoom);
    connect(m_ui->m_wndSpline, &SplineWidget::change, this, &CTrackViewCurveEditor::OnSplineChange);
    connect(m_ui->m_wndSpline, &SplineWidget::timeChange, this, &CTrackViewCurveEditor::OnSplineTimeMarkerChange);

    connect(m_ui->buttonTangentAuto,      &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_AUTO); });
    connect(m_ui->buttonTangentInZero,    &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_IN_ZERO); });
    connect(m_ui->buttonTangentInStep,    &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_IN_STEP); });
    connect(m_ui->buttonTangentInLinear,  &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_IN_LINEAR); });
    connect(m_ui->buttonTangentOutZero,   &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_OUT_ZERO); });
    connect(m_ui->buttonTangentOutStep,   &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_OUT_STEP); });
    connect(m_ui->buttonTangentOutLinear, &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_OUT_LINEAR); });
    connect(m_ui->buttonSplineFitX,       &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_SPLINE_FIT_X); });
    connect(m_ui->buttonSplineFitY,       &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_SPLINE_FIT_Y); });
    connect(m_ui->buttonSplineSnapGridX,  &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_SPLINE_SNAP_GRID_X); });
    connect(m_ui->buttonSplineSnapGridY,  &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_SPLINE_SNAP_GRID_Y); });
    connect(m_ui->buttonTangentUnify,     &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_TANGENT_UNIFY); });
    connect(m_ui->buttonFreezeKeys,       &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_FREEZE_KEYS); });
    connect(m_ui->buttonFreezeTangents,   &QToolButton::clicked, this, [&]() {OnSplineCmd(ID_FREEZE_TANGENTS); });

    ResetSliderRange();
}

//////////////////////////////////////////////////////////////////////////
CTrackViewCurveEditor::~CTrackViewCurveEditor()
{
    GetIEditor()->GetAnimation()->RemoveListener(this);
    GetIEditor()->UnregisterNotifyListener(this);
}



// CTrackViewGraph message handlers

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSequenceChanged(CTrackViewSequence* pSequence)
{
    UpdateSplines();
    update();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
    if (m_bIgnoreSelfEvents)
    {
        return;
    }
    switch (event)
    {
    case eNotify_OnCloseScene:
        m_ui->m_wndSpline->RemoveAllSplines();
        m_bLevelClosing = true;
        break;
    case eNotify_OnBeginNewScene:
    case eNotify_OnBeginSceneOpen:
        m_bLevelClosing = false;
        break;
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::UpdateSplines()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (!pSequence || m_bLevelClosing)
    {
        return;
    }

    CTrackViewTrackBundle selectedTracks;
    selectedTracks = pSequence->GetSelectedTracks();

    std::set<CTrackViewTrack*> oldTracks;
    for (auto iter = m_ui->m_wndSpline->GetTracks().begin(); iter != m_ui->m_wndSpline->GetTracks().end(); ++iter)
    {
        CTrackViewTrack* pTrack = *iter;
        oldTracks.insert(pTrack);
    }

    std::set<CTrackViewTrack*> newTracks;
    if (selectedTracks.AreAllOfSameType())
    {
        for (int i = 0; i < selectedTracks.GetCount(); i++)
        {
            CTrackViewTrack* pTrack = selectedTracks.GetTrack(i);

            if (pTrack->IsCompoundTrack())
            {
                unsigned int numChildTracks = pTrack->GetChildCount();
                for (unsigned int i = 0; i < numChildTracks; ++i)
                {
                    CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(i));
                    newTracks.insert(pChildTrack);
                }
            }
            else
            {
                newTracks.insert(pTrack);
            }
        }
    }

    if (oldTracks == newTracks)
    {
        return;
    }

    m_ui->m_wndSpline->RemoveAllSplines();
    for (auto iter = newTracks.begin(); iter != newTracks.end(); ++iter)
    {
        AddSpline(*iter);
    }

    UpdateTimeRange(pSequence);

    // If it is a rotation track, adjust the default value range properly to accommodate some degree values.
    if (selectedTracks.HasRotationTrack())
    {
        m_ui->m_wndSpline->SetDefaultValueRange(Range(-180.0f, 180.0f));
    }
    else
    {
        m_ui->m_wndSpline->SetDefaultValueRange(Range(-1.1f, 1.1f));
    }

    ResetSplineCtrlZoomLevel();

    return;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::AddSpline(CTrackViewTrack* pTrack)
{
    if (!pTrack->GetSpline())
    {
        return;
    }

    const int subTrackIndex = pTrack->GetSubTrackIndex();

    if (subTrackIndex >= 0)
    {
        QColor trackColor = QColor(255, 0, 0);
        switch (subTrackIndex)
        {
        case 0:
            trackColor = QColor(255, 0, 0);
            break;
        case 1:
            trackColor = QColor(0, 255, 0);
            break;
        case 2:
            trackColor = QColor(0, 0, 255);
            break;
        case 3:
            trackColor = QColor(255, 255, 0);
            break;
        }

        m_ui->m_wndSpline->AddSpline(pTrack->GetSpline(), pTrack, trackColor);
    }
    else
    {
        QColor    afColorArray[4];
        afColorArray[0] = QColor(255, 0, 0);
        afColorArray[1] = QColor(0, 255, 0);
        afColorArray[2] = QColor(0, 0, 255);
        afColorArray[3] = QColor(255, 0, 255); //Pink... so you know it's wrong if you see it.

        m_ui->m_wndSpline->AddSpline(pTrack->GetSpline(), pTrack, afColorArray);
    }
}

void CTrackViewCurveEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    OnSplineCmdUpdateUI();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSplineChange()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();
    if (pSequence)
    {
        pSequence->OnKeysChanged();
    }

    // In the end, focus this again in order to properly catch 'KeyDown' messages.
    m_ui->m_wndSpline->setFocus();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSplineCmd(UINT cmd)
{
    m_ui->m_wndSpline->OnUserCommand(cmd);
    OnSplineCmdUpdateUI();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSplineCmdUpdateUI()
{
    CTrackViewSequence* pSequence = GetIEditor()->GetAnimation()->GetSequence();

    if (m_bLevelClosing || !pSequence)
    {
        return;
    }

    m_ui->buttonSplineSnapGridX->setChecked(m_ui->m_wndSpline->IsSnapTime());
    m_ui->buttonSplineSnapGridY->setChecked(m_ui->m_wndSpline->IsSnapValue());
    m_ui->buttonTangentUnify->setChecked(m_ui->m_wndSpline->IsUnifiedKeyCurrentlySelected());
    m_ui->buttonFreezeKeys->setChecked(m_ui->m_wndSpline->IsKeysFrozen());
    m_ui->buttonFreezeTangents->setChecked(m_ui->m_wndSpline->IsTangentsFrozen());
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnTimeChanged(float newTime)
{
    m_ui->m_wndSpline->SetTimeMarker(newTime);
    m_ui->m_wndSpline->update();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::SetEditLock(bool bLock)
{
    m_ui->m_wndSpline->SetEditLock(bLock);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnTimelineChange()
{
    float fTime = m_timelineCtrl.GetTimeMarker();
    GetIEditor()->GetAnimation()->SetTime(fTime);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSplineTimeMarkerChange()
{
    float fTime = m_ui->m_wndSpline->GetTimeMarker();
    GetIEditor()->GetAnimation()->SetTime(fTime);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnHorizonSliderChange()
{
    int pos = m_ui->m_horizonSlider->value();
    Vec2 zoom = m_ui->m_wndSpline->GetZoom();

    // Zero value is not acceptable.
    zoom.x = max(SLIDERRANGE_TO_ZOOM(pos), 1.f / SLIDER_MULTIPLIER);
    m_ui->m_wndSpline->SetZoom(zoom);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnVerticalSliderChange()
{
    int pos = m_ui->m_verticlSlider->value();
    Vec2 zoom = m_ui->m_wndSpline->GetZoom();

    // Zero value is not acceptable.
    zoom.y = max(SLIDERRANGE_TO_ZOOM(pos), 1.f / SLIDER_MULTIPLIER);
    m_ui->m_wndSpline->SetZoom(zoom);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnSplineScrollZoom()
{
    ResetSliderRange();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::ResetSliderRange()
{
    Vec2 zoom = m_ui->m_wndSpline->GetZoom();
    Vec2 minValue = zoom / 2.f;
    Vec2 maxValue = zoom * 2.f;

    const QSignalBlocker sb1(m_ui->m_horizonSlider);
    const QSignalBlocker sb2(m_ui->m_verticlSlider);
    m_ui->m_horizonSlider->setRange(ZOOMRANGE_TO_SLIDER(minValue.x), ZOOMRANGE_TO_SLIDER(maxValue.x));
    m_ui->m_horizonSlider->setValue(ZOOMRANGE_TO_SLIDER((minValue.x + maxValue.x) / 2.f));

    m_ui->m_verticlSlider->setRange(ZOOMRANGE_TO_SLIDER(minValue.y), ZOOMRANGE_TO_SLIDER(maxValue.y));
    m_ui->m_verticlSlider->setValue(ZOOMRANGE_TO_SLIDER((minValue.y + maxValue.y) / 2.f));
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::SetFPS(float fps)
{
    m_timelineCtrl.SetFPS(fps);
}

//////////////////////////////////////////////////////////////////////////
float CTrackViewCurveEditor::GetFPS() const
{
    return m_timelineCtrl.GetFPS();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::SetTickDisplayMode(ETVTickMode mode)
{
    if (mode == eTVTickMode_InFrames)
    {
        m_timelineCtrl.SetMarkerStyle(TimelineWidget::MARKER_STYLE_FRAMES);
        m_ui->m_wndSpline->SetTooltipValueScale(GetFPS(), 1.0f);
    }
    else if (mode == eTVTickMode_InSeconds)
    {
        m_timelineCtrl.SetMarkerStyle(TimelineWidget::MARKER_STYLE_SECONDS);
        m_ui->m_wndSpline->SetTooltipValueScale(1.0f, 1.0f);
    }

    m_timelineCtrl.update();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::ResetSplineCtrlZoomLevel()
{
    m_ui->m_wndSpline->FitSplineToViewHeight();
    m_ui->m_wndSpline->FitSplineToViewWidth();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnKeysChanged(CTrackViewSequence* pSequence)
{
    m_ui->m_wndSpline->update();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnKeyAdded(CTrackViewKeyHandle& addedKeyHandle)
{
    EAnimCurveType trType = addedKeyHandle.GetTrack()->GetCurveType();
    if (trType == eAnimCurveType_BezierFloat)
    {
        // we query the added key's track to find the default tangent flags to use for newly created keys
        const int tangentFlagsForNewKeys = addedKeyHandle.GetTrack()->GetAnimNode()->GetDefaultKeyTangentFlags();
        I2DBezierKey bezierKey;
        addedKeyHandle.GetKey(&bezierKey);

        // clear any existing in and out tangent flags
        bezierKey.flags &= ~SPLINE_KEY_TANGENT_ALL_MASK;

        // set tangent flags to the default tangent flags used for the track's animNode and save them
        bezierKey.flags |= tangentFlagsForNewKeys;
        addedKeyHandle.SetKey(&bezierKey);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnKeySelectionChanged(CTrackViewSequence* pSequence)
{
    m_ui->m_wndSpline->update();

    m_ui->buttonTangentUnify->setChecked(m_ui->m_wndSpline->IsUnifiedKeyCurrentlySelected());
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type)
{
    if (isVisible() && type == ITrackViewSequenceListener::eNodeChangeType_Removed)
    {
        UpdateSplines();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewCurveEditor::OnNodeSelectionChanged(CTrackViewSequence* pSequence)
{
    if (isVisible())
    {
        UpdateSplines();
    }
}

void CTrackViewCurveEditor::OnSequenceSettingsChanged(CTrackViewSequence* pSequence)
{
    if (isVisible())
    {
        UpdateTimeRange(pSequence);
        m_timelineCtrl.update();
        m_ui->m_wndSpline->update();
    }
}

void CTrackViewCurveEditor::UpdateTimeRange(CTrackViewSequence* pSequence)
{
    Range timeRange =   pSequence->GetTimeRange();
    m_ui->m_wndSpline->SetTimeRange(timeRange);
    m_timelineCtrl.SetTimeRange(timeRange);
    m_ui->m_wndSpline->SetValueRange(Range(-2000.0f, 2000.0f));
}

void CTrackViewCurveEditor::SetPlayCallback(const std::function<void()>& callback)
{
    m_ui->m_wndSpline->SetPlayCallback(callback);
    m_timelineCtrl.SetPlayCallback(callback);
}

CTrackViewSplineCtrl& CTrackViewCurveEditor::GetSplineCtrl()
{
    return *m_ui->m_wndSpline;
}
