// Katlego Mosasi 48934828
// Mystic Manor - adventure game


#include <iostream>
#include <iomanip>
#include <string>
#include <ctime>
#include <cstdlib>
#include <cctype>
#include <cstring>

using namespace std;

// ---------------------- Structures ----------------------

struct Item {
    string name;
    string description;
    bool usable;        // can be "used" (e.g., key, potion)
    int healAmount;     // if usable and heals
    bool isKey;         // unlock doors
    bool isRelic;       // relic required to win
};

struct Enemy {
    string name;
    int hp;
    int attack;
    string taunt;
};

struct Room {
    string name;
    string description;
    // Connections: indices into the rooms array; -1 means no connection
    int north, south, east, west;
    Item* items[6];     // small fixed array for items in the room, NULL if empty
    int itemCount;
    Enemy* enemy;       // enemy pointer (NULL if none)
    bool locked;        // locked room (needs key or relic)
    string keyName;     // name of item that unlocks (if locked)
};

// ---------------------- Constants ----------------------

const int ROOM_COUNT = 6;
const int INVENTORY_CAP = 8;
const int MAX_ROOM_ITEMS = 6;

// ---------------------- Global game state ----------------------

Room rooms[ROOM_COUNT];
Item* allItems[16];   // for cleanup
int allItemsCount = 0;
Enemy* allEnemies[8];
int allEnemiesCount = 0;

Item* inventory[INVENTORY_CAP];
int invCount = 0;

Room* currentRoom = nullptr;

int playerHP = 100;
int playerAttack = 12;
int movesTaken = 0;

// Win condition: collect 3 relics (we'll use isRelic)
int relicsCollected() {
    int c = 0;
    for (int i = 0; i < invCount; ++i)
        if (inventory[i] && inventory[i]->isRelic) ++c;
    return c;
}

// ---------------------- Utility helpers ----------------------

void toLowerInPlace(string &s) {
    for (char &c : s) c = (char)tolower(c);
}

bool stringStartsWith(const string &s, const string &prefix) {
    if (s.size() < prefix.size()) return false;
    return s.substr(0, prefix.size()) == prefix;
}

Item* makeItem(const string& name, const string& desc, bool usable=false, int heal=0, bool key=false, bool relic=false) {
    Item* it = new Item();
    it->name = name;
    it->description = desc;
    it->usable = usable;
    it->healAmount = heal;
    it->isKey = key;
    it->isRelic = relic;
    allItems[allItemsCount++] = it;
    return it;
}

Enemy* makeEnemy(const string& name, int hp, int attack, const string& taunt="") {
    Enemy* e = new Enemy();
    e->name = name;
    e->hp = hp;
    e->attack = attack;
    e->taunt = taunt;
    allEnemies[allEnemiesCount++] = e;
    return e;
}

void placeItemInRoom(int roomIndex, Item* it) {
    Room &r = rooms[roomIndex];
    if (r.itemCount < MAX_ROOM_ITEMS) {
        r.items[r.itemCount++] = it;
    }
}

void removeItemFromRoom(Room &r, int idx) {
    if (idx < 0 || idx >= r.itemCount) return;
    // shift
    for (int i = idx; i + 1 < r.itemCount; ++i) r.items[i] = r.items[i+1];
    r.items[r.itemCount-1] = nullptr;
    r.itemCount--;
}

// Find item in current room by name (case-insensitive)
int findItemIndexInRoom(Room &r, const string &name) {
    string lname = name;
    toLowerInPlace(lname);
    for (int i = 0; i < r.itemCount; ++i) {
        string iname = r.items[i]->name;
        string inamel = iname;
        toLowerInPlace(inamel);
        if (inamel == lname) return i;
    }
    return -1;
}

int findItemIndexInInventory(const string &name) {
    string lname = name;
    toLowerInPlace(lname);
    for (int i = 0; i < invCount; ++i) {
        if (!inventory[i]) continue;
        string inamel = inventory[i]->name;
        toLowerInPlace(inamel);
        if (inamel == lname) return i;
    }
    return -1;
}

// Adds to inventory if space
bool addToInventory(Item* it) {
    if (invCount >= INVENTORY_CAP) return false;
    inventory[invCount++] = it;
    return true;
}

// Remove item from inventory index, shifting left
void removeFromInventory(int idx) {
    if (idx < 0 || idx >= invCount) return;
    for (int i = idx; i + 1 < invCount; ++i) inventory[i] = inventory[i+1];
    inventory[invCount-1] = nullptr;
    invCount--;
}

// Basic random within [min,max]
int rnd(int minVal, int maxVal) {
    return minVal + (rand() % (maxVal - minVal + 1));
}

// ---------------------- Game setup ----------------------

void initRoomsAndItems() {
    // Initialize rooms
    for (int i = 0; i < ROOM_COUNT; ++i) {
        rooms[i].name = "Unknown";
        rooms[i].description = "";
        rooms[i].north = rooms[i].south = rooms[i].east = rooms[i].west = -1;
        rooms[i].itemCount = 0;
        rooms[i].enemy = nullptr;
        rooms[i].locked = false;
        rooms[i].keyName = "";
        for (int j = 0; j < MAX_ROOM_ITEMS; ++j) rooms[i].items[j] = nullptr;
    }

    // Room 0: Grand Hall
    rooms[0].name = "Grand Hall";
    rooms[0].description = "A lofty hall with portraits whose eyes seem to follow you. Exits: east to Study, south to Kitchen, up to Tower (east & south).";
    rooms[0].east = 1; // Study
    rooms[0].south = 3; // Kitchen

    // Room 1: Study
    rooms[1].name = "Study";
    rooms[1].description = "Shelves of dusty books and a large oak desk. There's a locked chest here and a strange symbol on the floor.";
    rooms[1].west = 0;
    rooms[1].east = 2; // Library (east)
    rooms[1].locked = false;

    // Room 2: Library
    rooms[2].name = "Library";
    rooms[2].description = "Rows of old volumes. A ladder leads up, but that path is gone. A hidden alcove glows faintly.";
    rooms[2].west = 1;

    // Room 3: Kitchen
    rooms[3].name = "Kitchen";
    rooms[3].description = "An old kitchen. Pots hang from the ceiling, and a trapdoor lies partially concealed near the stove.";
    rooms[3].north = 0;
    rooms[3].south = 4; // Basement
    rooms[3].east = 5;  // Entrance to Courtyard/Tower path (treated as Tower)
    // Room 4: Basement
    rooms[4].name = "Basement";
    rooms[4].description = "A damp basement. The air tastes mineral-y. You notice strange markings.";
    rooms[4].north = 3;

    // Room 5: Tower (top)
    rooms[5].name = "Tower";
    rooms[5].description = "The tower room. Moonlight pours through a narrow window. A guardian shadows the center.";
    rooms[5].west = 3;
    rooms[5].locked = true; // Locked until relics/key
    rooms[5].keyName = "Tower Key"; // key required or relic combination

    // Create items
    Item* potion = makeItem("Small Potion", "A vial of red liquid. Restores a modest amount of health.", true, 25, false, false);
    Item* lantern = makeItem("Lantern", "An old oil lantern. Some dark corners require light.", false, 0, false, false);
    Item* rustyKey = makeItem("Rusty Key", "A small rusty key. Could open an old lock.", true, 0, true, false);
    Item* silverKey = makeItem("Silver Key", "Shines faintly. It feels important.", true, 0, true, false);
    Item* relicA = makeItem("Relic of Dawn", "A carved amulet with sun motifs. One of the ancient relics.", false, 0, false, true);
    Item* relicB = makeItem("Relic of Dusk", "An obsidian token etched with twilight shapes.", false, 0, false, true);
    Item* relicC = makeItem("Relic of Gloom", "A weathered charm humming with cold energy.", false, 0, false, true);
    Item* towerKey = makeItem("Tower Key", "A heavy key marked with the manor crest. It must open the tower.", true, 0, true, false);
    Item* mapPiece = makeItem("Map Piece", "A torn corner of a map showing the mansion's hidden rooms.", false, 0, false, false);
    Item* bread = makeItem("Stale Bread", "Not very nutritious, but better than nothing. Restores 6 HP.", true, 6, false, false);

    // Place items in rooms
    placeItemInRoom(0, lantern);
    placeItemInRoom(1, mapPiece);
    placeItemInRoom(1, rustyKey);
    placeItemInRoom(2, relicA);
    placeItemInRoom(3, bread);
    placeItemInRoom(3, potion);
    placeItemInRoom(4, relicB);
    placeItemInRoom(4, silverKey);
    placeItemInRoom(2, relicC); // library has two relics (one hidden alcove)
    placeItemInRoom(5, towerKey); // behind guardian maybe (but placed so player can find)

    // Create enemies
    Enemy* rat = makeEnemy("Giant Rat", 20, 6, "Squeak!");
    Enemy* wraith = makeEnemy("Wraith", 40, 10, "A whisper like winter...");
    Enemy* guardian = makeEnemy("Tower Guardian", 80, 14, "You should not be here, mortal!");
    // Place some enemies
    rooms[3].enemy = rat;
    rooms[4].enemy = wraith;
    rooms[5].enemy = guardian;
}

// Cleanup allocated memory
void cleanup() {
    for (int i = 0; i < allItemsCount; ++i) {
        delete allItems[i];
        allItems[i] = nullptr;
    }
    allItemsCount = 0;
    for (int i = 0; i < allEnemiesCount; ++i) {
        delete allEnemies[i];
        allEnemies[i] = nullptr;
    }
    allEnemiesCount = 0;
}

// ---------------------- Display / UI functions ----------------------

void showHeader() {
    cout << "====================================\n";
    cout << "       MYSTIC MANOR - ADVENTURE     \n";
    cout << "====================================\n";
    cout << "HP: " << playerHP << " | Relics: " << relicsCollected() << "/3 | Moves: " << movesTaken << "\n";
    cout << "Location: " << currentRoom->name << "\n\n";
}

void showHelp() {
    cout << "Available commands (type the word):\n"
         << "  go <north|south|east|west>  : Move between rooms\n"
         << "  look                        : Describe the room\n"
         << "  inspect <item>              : Examine an item in the room or inventory\n"
         << "  take <item>                 : Pick up an item\n"
         << "  drop <item>                 : Drop an item\n"
         << "  use <item>                  : Use an item in your inventory\n"
         << "  inv                         : Show inventory\n"
         << "  map                         : View a short map hint\n"
         << "  status                      : Show status (HP, relics)\n"
         << "  help                        : Show this help\n"
         << "  quit                        : Exit game\n";
}

// Show room contents
void describeCurrentRoom() {
    cout << currentRoom->description << "\n";
    if (currentRoom->itemCount > 0) {
        cout << "\nItems here:\n";
        for (int i = 0; i < currentRoom->itemCount; ++i) {
            cout << " - " << currentRoom->items[i]->name << "\n";
        }
    } else {
        cout << "\nNo visible items.\n";
    }
    if (currentRoom->enemy) {
        cout << "\nAn enemy looms: " << currentRoom->enemy->name << " -- " << currentRoom->enemy->taunt << "\n";
    }
    cout << "\nExits:";
    if (currentRoom->north != -1) cout << " north";
    if (currentRoom->south != -1) cout << " south";
    if (currentRoom->east  != -1) cout << " east";
    if (currentRoom->west  != -1) cout << " west";
    cout << "\n";
}

// Show inventory
void showInventory() {
    if (invCount == 0) {
        cout << "Inventory is empty.\n";
        return;
    }
    cout << "Inventory (" << invCount << "/" << INVENTORY_CAP << "):\n";
    for (int i = 0; i < invCount; ++i) {
        cout << i+1 << ". " << inventory[i]->name;
        if (inventory[i]->isRelic) cout << " (Relic)";
        if (inventory[i]->isKey) cout << " (Key)";
        cout << " - " << inventory[i]->description << "\n";
    }
}

// Short map hint (static)
void showMapHint() {
    cout << "\nMap hint (rooms indices):\n"
         << "   [2] Library\n"
         << "    |   \n"
         << "[1]Study - [0]Grand Hall - [3]Kitchen - [5]Tower\n"
         << "                 |\n"
         << "               [4]Basement\n\n";
}

// ---------------------- Game mechanics ----------------------

// Try to move in a direction. Returns whether move occurred.
bool movePlayer(const string& dir) {
    int nextIndex = -1;
    if (dir == "north") nextIndex = currentRoom->north;
    else if (dir == "south") nextIndex = currentRoom->south;
    else if (dir == "east")  nextIndex = currentRoom->east;
    else if (dir == "west")  nextIndex = currentRoom->west;
    else {
        cout << "Unknown direction.\n";
        return false;
    }

    if (nextIndex == -1) {
        cout << "You can't go that way.\n";
        return false;
    }

    // Check locked
    Room &target = rooms[nextIndex];
    if (target.locked) {
        // check if player has required key or has all relics
        bool unlocked = false;
        if (!target.keyName.empty()) {
            int keyIdx = findItemIndexInInventory(target.keyName);
            if (keyIdx != -1) {
                cout << "You use " << inventory[keyIdx]->name << " to unlock the door.\n";
                unlocked = true;
                // optionally consume key? We'll keep key.
            }
        }
        // Also allow opening tower if player has all relics
        if (!unlocked && relicsCollected() >= 3) {
            cout << "The relics you carry resonate with the lock. The way opens.\n";
            unlocked = true;
        }
        if (!unlocked) {
            cout << "The way is locked. You need '" << target.keyName << "' or the relics to access.\n";
            return false;
        }
        target.locked = false; // unlock permanently
    }

    currentRoom = &rooms[nextIndex];
    movesTaken++;
    cout << "You move " << dir << " to the " << currentRoom->name << ".\n";

    // Encounter: if enemy present, start combat automatically (player may attempt to flee)
    if (currentRoom->enemy) {
        cout << "A hostile presence blocks your path!\n";
        // combat occurs when player issues 'attack' or 'use', but also can auto-initiate a short engagement
    }
    return true;
}

// Inspect item name either in room or inventory
void inspectItem(const string& name) {
    int idx = findItemIndexInRoom(*currentRoom, name);
    if (idx != -1) {
        Item* it = currentRoom->items[idx];
        cout << it->name << ": " << it->description << "\n";
        return;
    }
    idx = findItemIndexInInventory(name);
    if (idx != -1) {
        Item* it = inventory[idx];
        cout << it->name << ": " << it->description << "\n";
        return;
    }
    cout << "No such item here or in your inventory.\n";
}

// Take item from room into inventory
void takeItem(const string& name) {
    int idx = findItemIndexInRoom(*currentRoom, name);
    if (idx == -1) {
        cout << "Item not found here.\n";
        return;
    }
    Item* it = currentRoom->items[idx];
    if (!addToInventory(it)) {
        cout << "Your inventory is full. Drop something first.\n";
        return;
    }
    removeItemFromRoom(*currentRoom, idx);
    cout << "You take the " << it->name << ".\n";
    // Some items may trigger immediate events
    if (it->isRelic) {
        cout << "The relic hums faintly as you grasp it.\n";
    }
}

// Drop item from inventory into current room
void dropItem(const string& name) {
    int idx = findItemIndexInInventory(name);
    if (idx == -1) {
        cout << "You don't have that item.\n";
        return;
    }
    if (currentRoom->itemCount >= MAX_ROOM_ITEMS) {
        cout << "There's no space here to drop the item.\n";
        return;
    }
    Item* it = inventory[idx];
    placeItemInRoom((int)(currentRoom - rooms), it); // index of currentRoom
    removeFromInventory(idx);
    cout << "You drop the " << it->name << ".\n";
}

// Use item from inventory
void useItem(const string& name) {
    int idx = findItemIndexInInventory(name);
    if (idx == -1) {
        cout << "You don't possess that item.\n";
        return;
    }
    Item* it = inventory[idx];
    if (!it->usable) {
        cout << "You can't use the " << it->name << " right now.\n";
        return;
    }

    // Healing items
    if (it->healAmount > 0) {
        int heal = it->healAmount;
        playerHP += heal;
        if (playerHP > 100) playerHP = 100;
        cout << "You use " << it->name << " and recover " << heal << " HP. (HP: " << playerHP << ")\n";
        // consume potion or not? We'll consume small potion but keep food? Let's consume any consumable (healAmount>0)
        removeFromInventory(idx);
        return;
    }

    // Keys: using a key tries to unlock adjacent locked rooms that require that key
    if (it->isKey) {
        // check adjacent rooms
        bool used = false;
        int roomIndex = (int)(currentRoom - rooms);
        int adj[4] = { currentRoom->north, currentRoom->south, currentRoom->east, currentRoom->west };
        for (int i = 0; i < 4; ++i) {
            int ri = adj[i];
            if (ri == -1) continue;
            if (rooms[ri].locked && rooms[ri].keyName == it->name) {
                rooms[ri].locked = false;
                cout << "You use " << it->name << " to unlock the " << rooms[ri].name << ".\n";
                used = true;
                break;
            }
        }
        if (!used) {
            cout << "No adjacent lock matches that key.\n";
        }
        // Keys remain in inventory (non-consumable)
        return;
    }

    cout << "You fiddle with the " << it->name << " but nothing happens.\n";
}

// Combat function: returns whether player survived
bool combat(Enemy* enemy) {
    if (!enemy) return true;
    cout << "Combat begins: " << enemy->name << " (HP " << enemy->hp << ") vs You (HP " << playerHP << ")\n";
    while (enemy->hp > 0 && playerHP > 0) {
        cout << "\nChoose action: [attack] [use <item>] [flee]\n> ";
        string line;
        getline(cin, line);
        string cmd = line;
        // to lower first word
        for (char &c : cmd) c = (char)tolower(c);
        if (stringStartsWith(cmd, "attack")) {
            int damage = rnd(playerAttack - 3, playerAttack + 3);
            cout << "You attack and deal " << damage << " damage.\n";
            enemy->hp -= damage;
        } else if (stringStartsWith(cmd, "use ")) {
            string itemName = line.substr(4);
            if (itemName.size() == 0) { cout << "Use what?\n"; continue; }
            int id = findItemIndexInInventory(itemName);
            if (id == -1) { cout << "You don't have that item.\n"; continue; }
            Item* it = inventory[id];
            if (it->healAmount > 0) {
                cout << "You use " << it->name << " mid-battle and heal " << it->healAmount << " HP.\n";
                playerHP += it->healAmount;
                if (playerHP > 100) playerHP = 100;
                removeFromInventory(id);
            } else {
                cout << "Using " << it->name << " has no effect in this fight.\n";
            }
        } else if (stringStartsWith(cmd, "flee")) {
            // attempt flee: 50% success
            if (rnd(1, 100) <= 50) {
                cout << "You manage to flee!\n";
                return true; // player survives, enemy remains
            } else {
                cout << "Flee attempt fails!\n";
            }
        } else {
            cout << "Unknown action.\n";
            continue;
        }

        if (enemy->hp <= 0) {
            cout << enemy->name << " collapses.\n";
            return true;
        }

        // Enemy attacks
        int edmg = rnd(enemy->attack - 2, enemy->attack + 3);
        cout << enemy->name << " attacks and deals " << edmg << " damage.\n";
        playerHP -= edmg;
        if (playerHP <= 0) {
            cout << "You have been defeated.\n";
            return false;
        } else {
            cout << "Your HP: " << playerHP << " | Enemy HP: " << enemy->hp << "\n";
        }
    }
    return playerHP > 0;
}

// Try to engage enemy in current room
void tryEnemyEncounter() {
    if (!currentRoom->enemy) return;
    cout << "You encounter " << currentRoom->enemy->name << "!\n";
    cout << currentRoom->enemy->taunt << "\n";
    bool survived = combat(currentRoom->enemy);
    if (!survived) {
        // player dead
        cout << "You collapse in the " << currentRoom->name << ". Game over.\n";
        // We will end via main loop
        return;
    } else {
        // enemy defeated: remove enemy pointer and maybe drop loot
        cout << "You defeated " << currentRoom->enemy->name << ".\n";
        // chance to drop an item: small potion or tower key if guardian
        if (currentRoom->enemy->name == "Tower Guardian") {
            cout << "The guardian falls, revealing a heavy key on its chest.\n";
            Item* key = nullptr;
            // find the "Tower Key" in allItems (already exists). We'll just place the existing tower key item into room if not present.
            // We earlier placed tower key into room 5, but to ensure it's present after combat, place it if missing.
            int present = findItemIndexInRoom(*currentRoom, "Tower Key");
            if (present == -1) {
                // find pointer from allItems
                for (int i=0;i<allItemsCount;i++){
                    if (allItems[i] && allItems[i]->name == "Tower Key") { key = allItems[i]; break; }
                }
                if (key) placeItemInRoom((int)(currentRoom - rooms), key);
            }
        } else {
            // chance to drop small potion
            if (rnd(1,100) <= 50) {
                // find "Small Potion"
                Item* p = nullptr;
                for (int i = 0; i < allItemsCount; ++i) {
                    if (allItems[i] && allItems[i]->name == "Small Potion") { p = allItems[i]; break; }
                }
                if (p) {
                    cout << "The creature drops a Small Potion.\n";
                    placeItemInRoom((int)(currentRoom - rooms), p);
                }
            }
        }
        // remove enemy
        currentRoom->enemy = nullptr;
    }
}

// Check win condition after significant actions
bool checkWinCondition() {
    if (relicsCollected() >= 3 && currentRoom->name == "Tower" && !currentRoom->enemy) {
        cout << "\nAs you stand in the Tower with the three relics, they combine into a radiant sigil.\n";
        cout << "A hidden mechanism opens and the manor's curse lifts. You have freed Mystic Manor!\n";
        return true;
    }
    return false;
}

// ---------------------- Main game loop ----------------------

int main() {
    srand((unsigned)time(nullptr));

    initRoomsAndItems();
    currentRoom = &rooms[0]; // start in Grand Hall

    cout << "Welcome to Mystic Manor! Your goal: find and collect the 3 relics, then reach the Tower and end the curse.\n";
    cout << "Type 'help' for commands. Good luck!\n\n";

    bool gameOver = false;
    bool playerQuit = false;

    while (!gameOver && playerHP > 0) {
        showHeader();
        // automatic short prompts if enemy present
        if (currentRoom->enemy) {
            cout << "Danger: " << currentRoom->enemy->name << " is here. You may 'attack' or 'flee' when prompted.\n";
        }

        cout << "\n> ";
        string line;
        getline(cin, line);
        if (line.size() == 0) { continue; }
        // parse command
        string cmd = line;
        // get first token
        string token;
        {
            size_t pos = cmd.find(' ');
            if (pos == string::npos) token = cmd;
            else token = cmd.substr(0, pos);
            for (char &c : token) c = (char)tolower(c);
        }

        if (token == "help") {
            showHelp();
        } else if (token == "look") {
            describeCurrentRoom();
        } else if (token == "status") {
            cout << "HP: " << playerHP << ", Attack: " << playerAttack << ", Relics: " << relicsCollected() << "/3\n";
        } else if (token == "map") {
            showMapHint();
        } else if (token == "inv" || token == "inventory") {
            showInventory();
        } else if (token == "go") {
            // get direction
            string dir;
            size_t pos = line.find(' ');
            if (pos == string::npos) { cout << "Go where?\n"; continue; }
            dir = line.substr(pos+1);
            toLowerInPlace(dir);
            if (movePlayer(dir)) {
                // after moving, check for immediate enemy and auto-encounter
                if (currentRoom->enemy) {
                    tryEnemyEncounter();
                    if (playerHP <= 0) break;
                }
                // check win
                if (checkWinCondition()) { gameOver = true; break; }
            }
        } else if (token == "inspect") {
            size_t pos = line.find(' ');
            if (pos == string::npos) { cout << "Inspect what?\n"; continue; }
            string itemName = line.substr(pos+1);
            inspectItem(itemName);
        } else if (token == "take" || token == "get") {
            size_t pos = line.find(' ');
            if (pos == string::npos) { cout << "Take what?\n"; continue; }
            string itemName = line.substr(pos+1);
            takeItem(itemName);
            // immediate check: if relic picked then maybe show message
            if (relicsCollected() >= 3) {
                cout << "You have collected all 3 relics! Now find the Tower and confront the guardian.\n";
            }
        } else if (token == "drop") {
            size_t pos = line.find(' ');
            if (pos == string::npos) { cout << "Drop what?\n"; continue; }
            string itemName = line.substr(pos+1);
            dropItem(itemName);
        } else if (token == "use") {
            size_t pos = line.find(' ');
            if (pos == string::npos) { cout << "Use what?\n"; continue; }
            string itemName = line.substr(pos+1);
            useItem(itemName);
            // maybe using key unlocked adjacent room; no further auto-check here
        } else if (token == "attack") {
            if (!currentRoom->enemy) {
                cout << "There is nothing to attack here.\n";
            } else {
                bool alive = combat(currentRoom->enemy);
                if (!alive) {
                    cout << "You have fallen.\n";
                    break;
                } else {
                    currentRoom->enemy = nullptr;
                    // check win
                    if (checkWinCondition()) { gameOver = true; break; }
                }
            }
        } else if (token == "quit" || token == "exit") {
            cout << "Do you really want to quit? (yes/no): ";
            string ans;
            getline(cin, ans);
            toLowerInPlace(ans);
            if (ans == "yes" || ans == "y") { playerQuit = true; break; }
        } else {
            cout << "Unknown command. Type 'help' to see commands.\n";
        }

        // Quick check for death (e.g., from combat)
        if (playerHP <= 0) {
            cout << "You can feel your life slipping away...\n";
            break;
        }
    } // end main loop

    if (playerHP <= 0) {
        cout << "\nYou died in Mystic Manor. Try again and may your choices be wiser.\n";
    } else if (playerQuit) {
        cout << "\nYou leave Mystic Manor before finishing your quest. Maybe next time.\n";
    } else if (gameOver) {
        cout << "\nCONGRATULATIONS! You have completed the Mystic Manor adventure.\n";
    }

    // cleanup dynamic memory
    cleanup();
    return 0;
}
