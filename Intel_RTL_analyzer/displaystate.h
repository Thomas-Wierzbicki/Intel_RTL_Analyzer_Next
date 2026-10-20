#ifndef DISPLAYSTATE_H
#define DISPLAYSTATE_H
#include <QtCore/QObject>
#include "frequencyzoom.h"
class DisplayState : public QObject
{
    Q_OBJECT
public:
    explicit DisplayState(QObject* parent = nullptr) : QObject(parent) {}
    FrequencyView full{432.30, 434.70}, view{432.30, 434.70};
    bool paused = false, coupled = true;
    double minimumDb = -100, rangeDb = 100, offsetDb = 0;
    void setBounds(double lower, double upper) {
        if (!(upper > lower) || (full.lower == lower && full.upper == upper)) return;
        full = {lower, upper}; view = full; emit viewChanged();
    }
    void setView(FrequencyView next) {
        if (!(next.upper > next.lower)) return;
        next = panFrequency(next, full.lower, full.upper, 0);
        if (next.lower == view.lower && next.upper == view.upper) return;
        view = next; emit viewChanged();
    }
    void setPaused(bool value) {
        if (paused == value) return; paused = value; emit pauseChanged(value);
    }
    void setCoupled(bool value) {
        if (coupled == value) return; coupled = value; emit couplingChanged(value);
        if (value) emit viewChanged();
    }
    void setScale(double minimum, double range, double offset) {
        minimumDb = minimum; rangeDb = range; offsetDb = offset; emit scaleChanged();
    }
    const char* levelUnit() const { return offsetDb == 0 ? "dBFS" : "dBFS + Offset"; }
signals:
    void viewChanged();
    void pauseChanged(bool paused);
    void couplingChanged(bool coupled);
    void scaleChanged();
};
#endif
