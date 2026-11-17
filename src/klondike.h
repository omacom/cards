#pragma once

#include "solitaire.h"

// The rules of Klondike, with no Qt in them: thirteen piles of cards, the
// moves between them, and enough history to take every one back.
namespace Klondike {

// Pile numbers: the stock, the waste, a foundation per suit, seven columns.
enum { Stock = 0, Waste = 1, Foundation = 2, Tableau = 6, Piles = 13 };

// The stock turns three cards at a time, of which only the last can be played.
enum { Draw = 3 };

using Card = Solitaire::Card;
using Move = Solitaire::Move;

class Game : public Solitaire::Game {
public:
  explicit Game(std::uint32_t seed = 1);

  std::uint32_t seed() const override { return m_seed; }
  const std::vector<Card> &pile(int index) const override { return m_piles[index]; }
  const std::vector<Move> &moves() const override { return m_moves; }

  int kind() const override { return 0; }
  int suits() const override { return 4; }
  int wastePile() const override { return Waste; }
  int tableauStart() const override { return Tableau; }
  int pileCount() const override { return Piles; }
  int cardsTotal() const override { return 52; }

  // Whether the cards from `index` to the top of `from` may go onto `to`.
  bool canMove(int from, int index, int to) const override;
  std::vector<int> targets(int from, int index) const override;
  // Where a double click sends those cards: home if it can, else a column.
  int bestTarget(int from, int index) const override;

  bool play(int from, int to, int count) override;
  // Turns cards from the stock, or brings the waste back round when it is empty.
  bool drawStock() override;
  bool undo() override;

  int home() const override;
  bool won() const override { return home() == 52; }
  // Nothing is hidden any more, so the rest can only go one way.
  bool finishable() const override;
  bool finishStep() override;
  // The next move worth making, or nothing when there is none to be found.
  std::optional<Move> hint() const override;

private:
  // Whether a lone card has anywhere to go: its foundation or a column.
  bool placeable(const Card &card) const;
  // Whether turning the stock, all the way round, ever shows a card that can be played.
  bool stockHelps() const;
  std::uint32_t m_seed;
  std::vector<Card> m_piles[Piles];
  std::vector<Move> m_moves;
};

}
