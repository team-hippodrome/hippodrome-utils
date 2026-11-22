# What it is

During move resolution, the player can press Z to queue open the battle command select menu and decide their next move while move resolution is happening. That move will then be automatically be entered if the player doesn't cancel the queue.

Display State machine: 
```mermaid
flowchart TD
    Hidden -->|Z| Showing
    Showing -->|Z| Hidden
    Showing -->|A| Move_Select
    Showing -->|B| Switch_select
    Move_Select -->|Z| Hidden
    Switch_Select -->|Z| Hidden
    Move_Select -->|L| Showing
    Switch_Select -->|L| Showing
    Move_Select -->|C| Move_Queued
    Switch_Select -->|C| Switch_Queued
    Move_Queued -->|Z| Hidden
    Switch_Queued -->|Z| Hidden
    Move_Queued -->|L| Move_Select
    Switch_Queued -->|L| Switch_Select
```

An queued order is canceled:
- Any time the player presses Z, including during command selection
- If the player is still in any state other than `Move_Queued` or `Switch_Queued` when move resolution finishes
- The order becomes illegal (due to fainting, freeze, trapping, etc.)

Command selection should follow send the order as soon as possible (10 frames in zero).