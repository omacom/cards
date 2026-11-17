#pragma once

#include "solitaire.h"
#include <QJsonObject>
#include <QString>
#include <memory>

// Reading a saved game back, shared by the window and the headless commands
// so both accept exactly the same files.
namespace SaveFile {

// A game that has made this many moves stops recording more, so what is
// written can always be read back.
enum { MaxMoves = 100000, MaxBytes = 2 * 1024 * 1024 };

enum class Result { Missing, Unreadable, TooLarge, Damaged, Ok };

// Replays the save at `path` into a game of the kind the save names, dealt
// from its seed, and hands back everything else it holds in `data`. `problem`
// gets the system's words when it cannot be read.
Result read(const QString &path, std::unique_ptr<Solitaire::Game> *game, QJsonObject *data = nullptr, QString *problem = nullptr);

}
