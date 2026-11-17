#pragma once

#include "solitaire.h"

// The rules of Spider, with no Qt in them: nineteen piles of cards, the moves
// between them, and enough history to take every one back.
namespace Spider {

// Pile numbers: the stock, a place per finished run, then ten columns.
enum { Stock = 0, Home = 1, Tableau = 9, Piles = 19 };

// The stock deals a whole row at a time: a card to every column.
enum { Row = 10 };

using Card = Solitaire::Card;
using Move = Solitaire::Move;

class Game : public Solitaire::Game {
public:
  // `suits` is 1, 2, or 4: the same thirteen ranks in one, two, or all four
  // suits, doubled to 104 cards either way.
  Game(std::uint32_t seed = 1, int suits = 4);

  std::uint32_t seed() const override { return m_seed; }
  const std::vector<Card> &pile(int index) const override { return m_piles[index]; }
  const std::vector<Move> &moves() const override { return m_moves; }

  int kind() const override { return 1; }
  int suits() const override { return m_suits; }
  int wastePile() const override { return -1; }
  int tableauStart() const override { return Tableau; }
  int pileCount() const override { return Piles; }
  int cardsTotal() const override { return 104; }

  // Whether the cards from `index` to the top of `from` may go onto `to`.
  bool canMove(int from, int index, int to) const override;
  std::vector<int> targets(int from, int index) const override;
  // Where a double click sends those cards.
  int bestTarget(int from, int index) const override;

  bool play(int from, int to, int count) override;
  bool drawStock() override;
  bool undo() override;

  int home() const override;
  bool won() const override;
  // What is left still needs picking: nothing plays itself out here.
  bool finishable() const override { return false; }
  bool finishStep() override { return false; }
  // Moves every completed run it finds to the first free home pile, each as
  // its own recorded move, until a pass finds none left. A step of its own,
  // so the moves recorded before it replay without doing it twice.
  void settle() override;
  // Its own move, plus every finished run the deal can hold, since one
  // action can finish more than one of them.
  int actionRoom() const override { return 1 + cardsTotal() / 13; }
  std::optional<Move> hint() const override;

private:
  // The lowest index from which the cards to the top of `from` are one run:
  // all face up, one suit, and in rank order, or -1 when the top is face down.
  int runStart(int from) const;
  std::uint32_t m_seed;
  int m_suits;
  std::vector<Card> m_piles[Piles];
  std::vector<Move> m_moves;
};

}
