#pragma once

#include "engines/mugen/AirFile.h"

#include <QPair>

class QTextDocument;

namespace OFS::Mugen::AirEdits {

// Throw creator edits to an AIR document. Each one is a single undo step.

// With `joinPrevious` the change joins the previous undo step (digits typed in a row).
void setFrame(QTextDocument* document, const AirFrame& frame, bool joinPrevious = false);

// Splits the element at `tick`. A blank element "-1,0, 0,0, T" gets the rest of its time.
bool splitFrame(QTextDocument* document, const AirAction& action, qint64 tick);

// The removed element's time goes to the previous one.
bool removeFrame(QTextDocument* document, const AirAction& action, qint64 tick);

// Only an element's own boxes change, by their inclusive width and height.
int scaleContactBoxes(QTextDocument* document, const QList<QPair<int, int>>& sprites, double widthScale,
                      double heightScale);

void appendAnimation(QTextDocument* document, int number, const QString& name, int time);

} // namespace OFS::Mugen::AirEdits
