#include "backend.h"
#include "klondike.h"
#include "savefile.h"
#include "spider.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {
quint32 randomSeed() { return QRandomGenerator::global()->bounded(1u, 0xffffffffu); }
// The game asked for, dealt from the seed: Klondike unless Spider was named.
std::unique_ptr<Solitaire::Game> dealt(int game, int suits, quint32 seed) {
  if (game == 1) return std::make_unique<Spider::Game>(seed, suits);
  return std::make_unique<Klondike::Game>(seed);
}
}

Backend::Backend(const QString &savePath, QObject *parent) : QObject(parent), m_game(dealt(0, 4, randomSeed())), m_savePath(savePath) {
  if (m_savePath.isEmpty()) m_savePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/game.json";
  if (m_savePath != ":memory:") {
    QDir().mkpath(QFileInfo(m_savePath).absolutePath());
    m_saveLock = std::make_unique<QLockFile>(m_savePath + ".lock");
    m_saveLock->setStaleLockTime(0);
    if (!m_saveLock->tryLock(0)) {
      m_saveBlocked = true;
      m_error = tr("Another Cards window owns this saved game, or its folder is not writable.");
    }
  }
  // The clock is the game's own time, not a poll: it runs only while a game
  // is under way and its window is the one in front.
  m_clock.setInterval(1000);
  connect(&m_clock, &QTimer::timeout, this, [this] { ++m_elapsed; emit elapsedChanged(); });
  m_finish.setInterval(70);
  connect(&m_finish, &QTimer::timeout, this, &Backend::finishStep);
  if (ownsSave()) restore();
  refresh();
  // A game left while its last cards were going home carries on from there.
  if (m_game->finishable()) m_finish.start();
}

Backend::~Backend() {
  // The seconds since the last move are worth keeping too.
  if (canUndo()) save();
}

void Backend::refresh() {
  // A different game holds a different number of cards, so its entries are
  // made afresh whenever the count changes.
  if (m_cards.size() != m_game->cardsTotal()) {
    m_cards.clear();
    for (int id = 0; id < m_game->cardsTotal(); ++id) m_cards.append(QVariantMap());
  }
  m_pileInfo.clear();
  for (int pile = 0; pile < m_game->pileCount(); ++pile) {
    const auto &cards = m_game->pile(pile);
    const int size = int(cards.size());
    // The waste shows its last three cards side by side.
    const int waste = m_game->wastePile();
    const int fanned = pile == waste && waste >= 0 ? std::min(size, 3) : 0;
    int down = 0, up = 0;
    for (int index = 0; index < size; ++index) {
      m_cards[cards[index].id] = QVariantMap{{"pile", pile}, {"index", index}, {"up", cards[index].up}, {"down", down},
                                             {"upBelow", up}, {"fan", std::max(0, index - (size - fanned))},
                                             {"suit", cards[index].suit()}};
      cards[index].up ? ++up : ++down;
    }
    m_pileInfo.append(QVariantMap{{"count", size}, {"down", down}, {"up", up}});
  }
  runClock();
  emit changed();
  emit marksChanged();
}

void Backend::runClock() {
  const bool running = m_active && canUndo() && !won();
  if (running != m_clock.isActive()) running ? m_clock.start() : m_clock.stop();
}

void Backend::clearMarks() {
  m_selectedPile = m_selectedIndex = -1;
  m_targets.clear();
  m_hintFrom = m_hintIndex = m_hintTo = -1;
  m_stuck = false;
}

void Backend::played() {
  clearMarks();
  refresh();
  save();
  if (!m_game->finishable()) return;
  m_finish.start();
}

void Backend::finishStep() {
  if (m_game->finishStep()) refresh();
  if (!m_game->finishable()) {
    m_finish.stop();
    save();
  }
}

bool Backend::move(int from, int to, int count) {
  // The cap leaves room for what one action records whole, so a game near it
  // can still be read back.
  if (won() || moves() + m_game->actionRoom() > SaveFile::MaxMoves || !m_game->play(from, to, count)) return false;
  m_game->settle();
  played();
  return true;
}

void Backend::activate(int pile, int index) {
  if (won() || m_finish.isActive() || pile < 0 || pile >= m_game->pileCount()) return;
  if (pile == Solitaire::Stock) { draw(); return; }
  const auto &cards = m_game->pile(pile);
  if (m_selectedPile >= 0 && pile != m_selectedPile && m_game->canMove(m_selectedPile, m_selectedIndex, pile)) {
    move(m_selectedPile, pile, int(m_game->pile(m_selectedPile).size()) - m_selectedIndex);
    return;
  }
  // Only the top card of a pile above the columns can be picked up.
  if (pile < m_game->tableauStart()) index = int(cards.size()) - 1;
  const bool pickable = index >= 0 && index < int(cards.size()) && cards[index].up;
  const bool same = pile == m_selectedPile && index == m_selectedIndex;
  clearMarks();
  if (pickable && !same) {
    m_selectedPile = pile; m_selectedIndex = index;
    for (int target : m_game->targets(pile, index)) m_targets.append(target);
  }
  emit marksChanged();
}

bool Backend::autoPlay(int pile, int index) {
  if (won() || m_finish.isActive() || pile < 0 || pile >= m_game->pileCount()) return false;
  if (pile == Solitaire::Stock) { draw(); return true; }
  // A card in the top row beyond the waste stays there unless it is carried back by hand.
  if (pile > m_game->wastePile() && pile < m_game->tableauStart()) return false;
  const int size = int(m_game->pile(pile).size());
  if (pile < m_game->tableauStart()) index = size - 1;
  const int to = m_game->bestTarget(pile, index);
  return to >= 0 && move(pile, to, size - index);
}

bool Backend::drop(int pile, int index, int to) {
  if (won() || m_finish.isActive() || pile < 0 || pile >= m_game->pileCount() || !m_game->canMove(pile, index, to)) return false;
  return move(pile, to, int(m_game->pile(pile).size()) - index);
}

void Backend::draw() {
  if (won() || m_finish.isActive() || moves() + m_game->actionRoom() > SaveFile::MaxMoves) return;
  if (m_game->drawStock()) {
    m_game->settle();
    played();
  }
}

void Backend::cancelSelection() {
  clearMarks();
  emit marksChanged();
}

void Backend::undo() {
  m_finish.stop();
  if (!m_game->undo()) return;
  clearMarks();
  refresh();
  save();
}

void Backend::hint() {
  if (won() || m_finish.isActive()) return;
  clearMarks();
  if (const auto move = m_game->hint()) {
    const int waste = m_game->wastePile();
    m_hintFrom = move->from == waste && move->to == Solitaire::Stock ? Solitaire::Stock : move->from;
    m_hintIndex = move->to == Solitaire::Stock ? -1 : int(m_game->pile(move->from).size()) - move->count;
    if (move->from == Solitaire::Stock) m_hintIndex = int(m_game->pile(Solitaire::Stock).size()) - 1;
    m_hintTo = move->from == Solitaire::Stock || move->to == Solitaire::Stock ? -1 : move->to;
  } else {
    m_stuck = true;
  }
  emit marksChanged();
}

void Backend::newGame(quint32 seed, int game, int suits) {
  m_finish.stop();
  if (!ownsSave()) return;
  m_saveBlocked = false;
  m_error.clear();
  emit savingChanged();
  // A negative game keeps the one in play, and its suit count.
  const int kind = game < 0 ? m_game->kind() : game;
  const int count = game < 0 ? m_game->suits() : suits;
  m_game = dealt(kind, count, seed ? seed : randomSeed());
  m_elapsed = 0;
  emit elapsedChanged();
  clearMarks();
  refresh();
  save();
}

void Backend::dismissError() { m_error.clear(); emit savingChanged(); }

void Backend::setActive(bool value) {
  if (value == m_active) return;
  m_active = value;
  emit activeChanged();
  runClock();
}
void Backend::setSeries(int value) {
  if (value < 0 || value > 3 || value == m_series) return;
  m_series = value; emit seriesChanged(); save();
}

void Backend::save() {
  if (m_savePath == ":memory:" || m_saveBlocked || !ownsSave()) return;
  QJsonArray moves;
  for (const Solitaire::Move &move : m_game->moves()) moves.append(QJsonArray{move.from, move.to, move.count});
  // Klondike saves stay the version and shape they have always had, so an
  // older build opens them still; Spider names its game and suits on a
  // version of its own, since an older build must reject what it cannot
  // play rather than deal it as Klondike.
  const int version = m_game->kind() == 1 ? 2 : 1;
  QJsonObject data{{"version", version}, {"seed", qint64(m_game->seed())}, {"moves", moves},
                   {"elapsed", m_elapsed}, {"series", m_series}};
  if (version == 2) {
    data["game"] = m_game->kind();
    data["suits"] = m_game->suits();
  }
  QDir().mkpath(QFileInfo(m_savePath).absolutePath());
  QSaveFile file(m_savePath);
  file.setDirectWriteFallback(false);
  // Compact, so the longest game allowed still fits the size that is read back.
  const auto bytes = QJsonDocument(data).toJson(QJsonDocument::Compact);
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
    m_saveFailed = true;
    m_error = tr("Could not save your game: %1").arg(file.errorString()); emit savingChanged();
  } else if (m_saveFailed) {
    m_saveFailed = false; m_error.clear(); emit savingChanged();
  }
}

void Backend::restore() {
  if (m_savePath == ":memory:") return;
  QJsonObject data;
  QString problem;
  std::unique_ptr<Solitaire::Game> game;
  switch (SaveFile::read(m_savePath, &game, &data, &problem)) {
  case SaveFile::Result::Missing: return;
  case SaveFile::Result::Unreadable: m_saveBlocked = true; m_error = tr("Could not open your saved game: %1").arg(problem); return;
  case SaveFile::Result::TooLarge: m_saveBlocked = true; m_error = tr("The saved game is too large to open."); return;
  // Keep the damaged save intact until the player explicitly starts again.
  case SaveFile::Result::Damaged: m_saveBlocked = true; m_error = tr("The saved game is damaged. Start a new game to replace it."); return;
  case SaveFile::Result::Ok: break;
  }
  m_game = std::move(game);
  m_elapsed = std::clamp(data["elapsed"].toInt(), 0, 359999);
  m_series = std::clamp(data["series"].toInt(), 0, 3);
}
