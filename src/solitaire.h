#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// The parts of the games the window shows that they hold in common: a card, a
// move, and the surface the backend, the saved file, and the headless commands
// call. Klondike and Spider each fill it in; neither knows the other.
namespace Solitaire {

// Every game keeps its stock in pile 0.
enum { Stock = 0 };

// Ids name a card's rank and, usually, its suit: thirteen spades, hearts,
// diamonds, then clubs, ace first, and the second deck follows. So rank is
// id % 13 and suit is (id / 13) % 4, unless a game says otherwise: Spider's
// easier deals repeat the same ranks in fewer suits, so a card carries its
// suit when it is not the one its id names.
struct Card {
  int id = 0;
  bool up = false;
  int suitId = -1;
  int suit() const { return suitId < 0 ? (id / 13) % 4 : suitId; }
  int rank() const { return id % 13; }
  bool red() const { return suit() == 1 || suit() == 2; }
};

// `count` cards off the top of `from` onto `to`. `flip` records that the move
// turned the card it uncovered, so undo can turn it back.
struct Move {
  int from = -1, to = -1, count = 0;
  bool flip = false;
  bool operator==(const Move &other) const { return from == other.from && to == other.to && count == other.count; }
};

class Game {
public:
  virtual ~Game() = default;

  virtual std::uint32_t seed() const = 0;
  virtual const std::vector<Card> &pile(int index) const = 0;
  virtual const std::vector<Move> &moves() const = 0;

  // Which game this is, 0 Klondike and 1 Spider, and how many suits it deals.
  virtual int kind() const = 0;
  virtual int suits() const = 0;
  // Where the piles lie: the waste is Klondike's alone, so a game without one
  // answers -1; the columns start at `tableauStart`; `cardsTotal` is what a
  // won game has put home, 52 or 104.
  virtual int wastePile() const = 0;
  virtual int tableauStart() const = 0;
  virtual int pileCount() const = 0;
  virtual int cardsTotal() const = 0;

  // Whether the cards from `index` to the top of `from` may go onto `to`.
  virtual bool canMove(int from, int index, int to) const = 0;
  virtual std::vector<int> targets(int from, int index) const = 0;
  // Where a double click sends those cards: home if it can, else a column.
  virtual int bestTarget(int from, int index) const = 0;

  virtual bool play(int from, int to, int count) = 0;
  // Turns cards out of the stock; what a draw is worth is the game's business.
  virtual bool drawStock() = 0;
  virtual bool undo() = 0;

  virtual int home() const = 0;
  virtual bool won() const = 0;
  // Whether what is left can only go one way now, and one step of playing it out.
  virtual bool finishable() const = 0;
  virtual bool finishStep() = 0;
  // A game's own finishing touches on the move just played — the finished
  // runs Spider deals home — as a step of its own, so a recorded move can be
  // replayed without them and the history stays the whole truth of how the
  // table came to be. Games without such touches do nothing here.
  virtual void settle() {}
  // The most moves one action can put on the record: its own move, plus what
  // the game does about it afterwards. The save's move cap leaves this much
  // room, so an action near the cap is recorded whole or not at all.
  virtual int actionRoom() const { return 1; }
  // The next move worth making, or nothing when there is none to be found.
  virtual std::optional<Move> hint() const = 0;
};

}
