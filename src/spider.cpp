#include "spider.h"

#include <algorithm>

namespace Spider {

using Solitaire::Card;
using Solitaire::Move;

Game::Game(std::uint32_t seed, int suits) : m_seed(seed), m_suits(suits == 1 || suits == 2 ? suits : 4) {
  // The shuffle is spelled out rather than left to the standard library, so
  // a saved seed deals the same game on every machine and compiler.
  std::uint64_t state = seed;
  auto next = [&state] {
    std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  };
  // A four-suit deal is two full decks, a two-suit one four decks of spades
  // and hearts, and a one-suit one eight decks of spades. The suit rides on
  // the card, since an id alone names the right suit only for four.
  Card deck[104];
  for (int i = 0; i < 104; ++i) deck[i] = {i, false, (i / 13) % m_suits};
  for (int i = 103; i > 0; --i) std::swap(deck[i], deck[next() % (i + 1)]);
  // Ten columns take fifty-four cards row by row, five full rows and one
  // more for each of the first four columns, all face down until the deal
  // turns each column's top card up; the rest waits in the stock.
  int dealt = 0;
  for (int row = 0; row < 5; ++row)
    for (int column = 0; column < Row; ++column) m_piles[Tableau + column].push_back(deck[dealt++]);
  for (int column = 0; column < 4; ++column) m_piles[Tableau + column].push_back(deck[dealt++]);
  for (int column = 0; column < Row; ++column) m_piles[Tableau + column].back().up = true;
  while (dealt < 104) m_piles[Stock].push_back(deck[dealt++]);
}

bool Game::canMove(int from, int index, int to) const {
  if (from < Tableau || from >= Piles || to < Tableau || to >= Piles || from == to) return false;
  const int start = runStart(from);
  if (start < 0 || index < start || index >= int(m_piles[from].size())) return false;
  const auto &target = m_piles[to];
  if (target.empty()) return true;
  return target.back().up && target.back().rank() == m_piles[from][index].rank() + 1;
}

std::vector<int> Game::targets(int from, int index) const {
  std::vector<int> result;
  for (int to = Tableau; to < Piles; ++to)
    if (canMove(from, index, to)) result.push_back(to);
  return result;
}

int Game::bestTarget(int from, int index) const {
  if (from < Tableau || from >= Piles || index < 0 || index >= int(m_piles[from].size())) return -1;
  const int start = runStart(from);
  if (start < 0 || index < start) return -1;
  const int suit = m_piles[from][index].suit();
  int other = -1;
  int empty = -1;
  for (int to = Tableau; to < Piles; ++to) {
    if (!canMove(from, index, to)) continue;
    const auto &target = m_piles[to];
    // A run grown on in its own suit stays together for when it can leave whole.
    if (!target.empty() && target.back().suit() == suit) return to;
    if (!target.empty() && other < 0) other = to;
    if (target.empty() && empty < 0) empty = to;
  }
  if (other >= 0) return other;
  // A run already at the foot of its column gains nothing from another empty one.
  if (empty >= 0 && index != 0) return empty;
  return -1;
}

bool Game::play(int from, int to, int count) {
  Move move{from, to, count, false};
  if (from == Stock) {
    // A draw deals a card off the top of the stock to every column, left to
    // right, and only when none of them is an empty hole to deal onto.
    auto &stock = m_piles[Stock];
    if (to != Tableau || count != Row || int(stock.size()) < Row) return false;
    for (int column = Tableau; column < Piles; ++column)
      if (m_piles[column].empty()) return false;
    for (int column = Tableau; column < Piles; ++column) {
      m_piles[column].push_back({stock.back().id, true, stock.back().suitId});
      stock.pop_back();
    }
  } else if (to >= Home && to < Tableau) {
    // A finished run, king down to ace in one suit, leaves for an empty home
    // pile whole, and the card it uncovers is turned up as part of the move.
    if (from < Tableau || from >= Piles || count != 13 || !m_piles[to].empty()) return false;
    auto &source = m_piles[from];
    if (int(source.size()) < 13) return false;
    const int base = int(source.size()) - 13;
    for (int k = 0; k < 13; ++k)
      if (!source[base + k].up || source[base + k].suit() != source[base].suit() || source[base + k].rank() != 12 - k) return false;
    m_piles[to].insert(m_piles[to].end(), source.begin() + base, source.end());
    source.resize(base);
    if (!source.empty() && !source.back().up) {
      source.back().up = true;
      move.flip = true;
    }
  } else {
    if (from < 0 || from >= Piles || count < 1 || count > int(m_piles[from].size())) return false;
    auto &source = m_piles[from];
    const int index = int(source.size()) - count;
    if (!canMove(from, index, to)) return false;
    m_piles[to].insert(m_piles[to].end(), source.begin() + index, source.end());
    source.resize(index);
    if (from >= Tableau && !source.empty() && !source.back().up) {
      source.back().up = true;
      move.flip = true;
    }
  }
  m_moves.push_back(move);
  return true;
}

bool Game::drawStock() {
  // Unlike Klondike there is no waste to bring round, and Spider refuses to
  // deal onto a hole, so an empty stock or an empty column ends the draw.
  if (m_piles[Stock].empty()) return false;
  for (int column = Tableau; column < Piles; ++column)
    if (m_piles[column].empty()) return false;
  return play(Stock, Tableau, Row);
}

bool Game::undo() {
  if (m_moves.empty()) return false;
  const Move move = m_moves.back();
  m_moves.pop_back();
  if (move.from == Stock) {
    // A draw handed its cards out left to right, so they go back right to
    // left, face down, and the stock reads as it did before the draw. Every
    // column is sure to be holding its card, since a draw onto an empty one
    // is refused.
    for (int column = Row - 1; column >= 0; --column) {
      auto &pile = m_piles[Tableau + column];
      m_piles[Stock].push_back({pile.back().id, false, pile.back().suitId});
      pile.pop_back();
    }
    return true;
  }
  auto &from = m_piles[move.from];
  auto &to = m_piles[move.to];
  if (move.flip) from.back().up = false;
  from.insert(from.end(), to.end() - move.count, to.end());
  to.resize(to.size() - move.count);
  return true;
}

int Game::home() const {
  int total = 0;
  for (int pile = Home; pile < Tableau; ++pile) total += int(m_piles[pile].size());
  return total;
}

bool Game::won() const { return home() == 104; }

int Game::runStart(int from) const {
  const auto &pile = m_piles[from];
  int index = int(pile.size()) - 1;
  if (index < 0 || !pile[index].up) return -1;
  const int suit = pile[index].suit();
  // A run is its cards in rank order as well as one suit, so the walk stops
  // at the first card that breaks the countdown.
  while (index > 0 && pile[index - 1].up && pile[index - 1].suit() == suit
         && pile[index - 1].rank() == pile[index].rank() + 1)
    --index;
  return index;
}

void Game::settle() {
  for (bool found = true; found;) {
    found = false;
    for (int from = Tableau; from < Piles && !found; ++from)
      for (int to = Home; to < Tableau && !found; ++to)
        if (m_piles[to].empty() && play(from, to, 13)) found = true;
  }
}

std::optional<Move> Game::hint() const {
  // A run that just moved is there to be built on, not sent straight back to
  // the pile it came from.
  const Move last = m_moves.empty() ? Move{} : m_moves.back();
  const bool justMoved = last.from >= Tableau && last.from < Piles && last.to >= Tableau && last.to < Piles;

  // A run that leaves turning the card beneath it, or a whole column that
  // leaves for elsewhere: both are steps that cannot simply be taken back.
  for (int from = Tableau; from < Piles; ++from) {
    const auto &pile = m_piles[from];
    const int start = runStart(from);
    if (start < 0) continue;
    for (int index = start; index < int(pile.size()); ++index) {
      if (index > 0 && pile[index - 1].up) continue;
      for (int to = Tableau; to < Piles; ++to) {
        if (index == 0 && m_piles[to].empty()) continue;
        if (!canMove(from, index, to)) continue;
        if (justMoved && from == last.to && to == last.from) continue;
        return Move{from, to, int(pile.size()) - index, false};
      }
    }
  }
  // A run grown on in its own suit keeps more of a column together for when
  // a whole run can leave.
  for (int from = Tableau; from < Piles; ++from) {
    const auto &pile = m_piles[from];
    const int start = runStart(from);
    if (start < 0) continue;
    for (int index = start; index < int(pile.size()); ++index) {
      for (int to = Tableau; to < Piles; ++to) {
        const auto &target = m_piles[to];
        if (target.empty() || !canMove(from, index, to)) continue;
        if (justMoved && from == last.to && to == last.from) continue;
        if (target.back().suit() == pile[index].suit()) return Move{from, to, int(pile.size()) - index, false};
      }
    }
  }
  // Any other legal move of a run onto a card: even one that only reshapes a
  // column can be what frees the next card.
  for (int from = Tableau; from < Piles; ++from) {
    const auto &pile = m_piles[from];
    const int start = runStart(from);
    if (start < 0) continue;
    for (int index = start; index < int(pile.size()); ++index) {
      for (int to = Tableau; to < Piles; ++to) {
        if (m_piles[to].empty() || !canMove(from, index, to)) continue;
        if (justMoved && from == last.to && to == last.from) continue;
        return Move{from, to, int(pile.size()) - index, false};
      }
    }
  }
  // Turning the stock, when there is a whole row to deal and no column is an
  // empty hole to deal onto.
  if (int(m_piles[Stock].size()) >= Row) {
    bool hole = false;
    for (int column = Tableau; column < Piles; ++column)
      if (m_piles[column].empty()) hole = true;
    if (!hole) return Move{Stock, Tableau, Row, false};
  }
  return std::nullopt;
}

}
