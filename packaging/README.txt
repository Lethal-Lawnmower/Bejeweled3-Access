Bejeweled 3 Access, version 0.1
================================

This is Bejeweled 3 Access. This is more a tech experiment for me, since
we've never, to my knowledge, had any good mainstream hits in this genre
before. The project is public for transparency and so you can follow the
active development, and add and/or fix changes you specifically want:
https://github.com/Lethal-Lawnmower/Bejeweled3-Access

Do not purchase the game if you are hesitant or unsure about this mod. It
only works with the Steam version on its latest version.

Timed modes are excluded for the time being till we get the gameplay ironed
out to the way we like.


What it does
------------

A screen-reader accessibility mod for Bejeweled 3 (Steam). It speaks the
menus, dialogs and the game board through your screen reader (via the Prism
speech library; tested with NVDA), adds keyboard navigation to the menus, and
describes the board while you play with the game's own keyboard controls.
It uses the game's own text wherever the game has it.


Install
-------

1. Close Bejeweled 3.
2. Open the game folder. In Steam: right-click Bejeweled 3, Manage, Browse
   local files. Usually:
   C:\Program Files (x86)\Steam\steamapps\common\Bejeweled 3
3. Copy winmm.dll and prism.dll into that folder (next to Bejeweled3.exe).
4. Start the game from Steam. You should hear
   "Bejeweled 3 access 0.1 loaded".
   If you hear that your version is not supported, the mod stays off: it
   only works with the current Steam version of the game.

Uninstall: delete winmm.dll and prism.dll from the game folder.


Keys in menus and dialogs
-------------------------

Up / Down, or Tab / Shift+Tab   move between items
Enter or Space                  press a button, tick a checkbox, pick a tab
Left / Right                    change a slider
Home / End                      first / last item
F5                              repeat where you are
F6                              game status during play
F1                              list the keys

In text fields (your name), type normally; the mod reads what you type.


Keys on the game board
----------------------

The game's own controls:
Arrows                          move the cursor (first press shows it)
Space or Enter                  select the gem under the cursor; an arrow
                                then swaps it in that direction
W, A, S, D                      swap the gem under the cursor up, left,
                                down or right
Escape                          game menu

Added by the mod:
(cursor movement)               the gem under the cursor is spoken
E                               where am I: gem and position
Q                               the four gems around the cursor
H                               the game's hint: which gem it points to
X                               next special gem; press again for the
                                one after
T                               time left (timed modes)
M                               mode details (poker hand, butterflies,
                                ice columns, ...)
F6                              score, level, progress
F1                              list the board keys
Tab, then Enter                 the board's own buttons (hint, menu)
R / C                           read the row / column (off by default)

After each move you hear what moved where, and when the board has settled
a short summary: points, gems cleared, cascades, special gems made or set
off. Swaps that make no match are reported as swapped back.


Settings
--------

On first start the mod creates a settings file with an explanation for
every option:
%LOCALAPPDATA%\Bejeweled3Access\settings.ini
(paste that into the Windows Run box or Explorer's address bar). Edit it
while the game is closed. Options include what the cursor says, position
style, how much is said after a move, hints, extra reading keys and sound
cues.

A log of what the mod said is written to
%LOCALAPPDATA%\Bejeweled3Access\mod.log (useful when reporting problems).


Known limitations in 0.1
------------------------

- Steam version only; any other version is refused.
- Not supported for now: Lightning, Ice Storm, Diamond Mine and the timed
  quests.
- Butterflies, Poker and the move-based quests are untested.
- Settings can only be changed in the settings file.
- Tested with NVDA on one PC so far.


Licences
--------

Bejeweled 3 Access is free software under the GNU General Public License
version 3 (licences\LICENSE-Bejeweled3-Access.txt). Source code:
https://github.com/Lethal-Lawnmower/Bejeweled3-Access

prism.dll is Prism v0.18.3 by Ethin Probst and contributors, licensed under
the Mozilla Public License 2.0 (licences\prism). Its source code is
available at https://github.com/ethindp/prism (tag v0.18.3); this copy was
built unmodified as 32-bit.

This is an unofficial fan accessibility mod. It contains no game files.
Bejeweled is a trademark of its owners.
