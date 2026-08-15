#pragma once

#include "engines/mugen/AirFile.h"

class QTextDocument;

namespace OFS::Mugen::AirEdits {

// Throw creator edits to an AIR document. Each one is a single undo step.

void setFrame(QTextDocument* document, const AirFrame& frame);

// Splits the element at `tick`. A blank element "-1,0, 0,0, T" gets the rest of its time.
bool splitFrame(QTextDocument* document, const AirAction& action, qint64 tick);

// The removed element's time goes to the previous one.
bool removeFrame(QTextDocument* document, const AirAction& action, qint64 tick);

void appendAnimation(QTextDocument* document, int number, const QString& name, int time);

} // namespace OFS::Mugen::AirEdits
