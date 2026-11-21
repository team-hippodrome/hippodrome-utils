# What it is
Side Summary Bars are UI elements supported by Hippodrome Ports that display the status of the Pokemon on both sides of a battle. Summary bars per sides are made of *cards* that detail individual pokemon. 

# Attributes of card

Cards comprise of:
1. A state
2. The 3d model of the pokemon
3. HP bar
4. A view of known moves of the pokemon
5. A listing of the (estimated) speed stat of a pokemon

We call the  pokemon the card stores information of its referent. 

## State
A card can have the following stats:
- Picked (selected on the order select screen)
- Not Picked (not Picked for the battle in formats where you bring less than your full party)
- Statused (volatile Status condition like poison, sleep)
- Fainted
- Unknown (for when the opposing side has not reveal all pokemon picked for battle)

## Model
The model of the pokemon should be the same model as if they pokemon where active in battle.
- The model shall be in color and in idle animation if the card is Picked
- The model shall have a slight black hue  in idle animation if the card is Unknown
- - The model shall have a darker black hue and frozen at current or initial idle animation phase if the card is Not picked
- The model shall be colored in the color of the status (light blue for sleep, purple for poison, deep purple for toxic, yellow for paralysis, aqua for freeze, red for burn) if it is Statused
  - sleeping or frozen referents should have their models play their sleeping or frozen animation 
  - All other status aliments should have the model's idle animation play at half speed.
- Upon the model's reference fainting, the model shall play the fainting animation and then stop all motion, with a grey coloration afterwards

The model should update upon using transform. The model should NOT update upon having an active substitute.
The model is on the left side of the card.

Large pokemon such as onix should have their card's model's scale changes so that the face(s) of hte pokemon displays prominently when not fainted.

## HP Bar
The HP bar is the first UI 

## Moves
All moves are revealed on the player's Bar in single player. Moves on the COM's side are reveal only as they are used, or if trainer memory is on, according to the save files' trainer memory.

Moves are only revealed as they are used in multiplayer.

## Speed stat estimate
see ./SPEED_STAT_ESTIMATE.md