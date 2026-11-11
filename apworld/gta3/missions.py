from dataclasses import dataclass

# The three islands, in the order the story opens them; each one is a region of the apworld.
PORTLAND = "Portland"
STAUNTON = "Staunton Island"
SHORESIDE = "Shoreside Vale"


@dataclass(frozen=True)
class Contact:
    """Someone who gives missions: in ordered mode, each copy of their progressive item unlocks their next mission."""
    name: str
    progressive_item: str
    progressive_id: int


@dataclass(frozen=True)
class Mission:
    """A story mission and everything the apworld, the client and the mod need to know about it."""
    name: str
    location_id: int
    # The item that unlocks it in free mode; 0 for Give Me Liberty, which the intro starts by itself.
    unlock_id: int
    # The text key the game registers when it is passed, which the mod sends as its check ("LM2").
    key: str
    # Its number in main.scm, as LOAD_AND_LAUNCH_MISSION_INTERNAL numbers it: the mod unlocks its marker by it.
    number: int
    contact: str
    # The farthest island it is played on: Portland is always open, the others once the story opens them.
    island: str
    # The missions whose pass starts its trigger in the game: ordered mode requires them, free mode does not, as the mod starts the trigger itself.
    requires: tuple[str, ...] = ()


# Stable numbers shared by the server, the client and the mod: never renumber them, never reuse a retired one.
# Retired ids: location 2 (Luigi's Girls) and item 3 (Luigi's Girls Unlock), merged into Give Me Liberty, which the game plays as one mission with Luigi's Girls.
CONTACTS = [
    Contact("Luigi", "Progressive Luigi Mission", 1),
    Contact("Joey", "Progressive Joey Mission", 6),
    Contact("Toni", "Progressive Toni Mission", 7),
    Contact("Salvatore", "Progressive Salvatore Mission", 8),
    Contact("Marty", "Progressive Marty Mission", 9),
    Contact("El Burro", "Progressive El Burro Mission", 10),
    Contact("Asuka", "Progressive Asuka Mission", 11),
    Contact("Kenji", "Progressive Kenji Mission", 12),
    Contact("Ray", "Progressive Ray Mission", 13),
    Contact("Donald Love", "Progressive Donald Love Mission", 14),
    Contact("King Courtney", "Progressive King Courtney Mission", 15),
    Contact("D-Ice", "Progressive D-Ice Mission", 16),
    Contact("Catalina", "Progressive Catalina Mission", 17),
]

# The filler item, given in the game as money.
MONEY_ITEM = "$10000"
MONEY_ID = 2
MONEY_AMOUNT = 10000

# Every story mission, each contact's in story order (the order its progressive item unlocks them). The requires come from main.scm:
# the mission whose passed routine runs START_NEW_SCRIPT for its trigger, or for the phone script that starts it (El Burro, King Courtney, D-Ice).
# Pump-Action Pimp, The Fuzz Ball, Dead Skunk in the Trunk and The Getaway also require Chaperone: their triggers end themselves once
# Salvatore's Called a Meeting is passed while Chaperone is not, and only Chaperone's passed routine starts them again.
MISSIONS = [
    Mission("Give Me Liberty", 1, 0, "LM1", 19, "Luigi", PORTLAND),
    Mission("Don't Spank Ma Bitch Up", 3, 4, "LM2", 20, "Luigi", PORTLAND, ("Give Me Liberty",)),
    Mission("Drive Misty For Me", 4, 5, "LM3", 21, "Luigi", PORTLAND, ("Don't Spank Ma Bitch Up",)),
    Mission("Pump-Action Pimp", 5, 18, "LM4", 22, "Luigi", PORTLAND, ("Drive Misty For Me", "Chaperone")),
    Mission("The Fuzz Ball", 6, 19, "LM5", 23, "Luigi", PORTLAND, ("Pump-Action Pimp", "Chaperone")),
    Mission("Mike Lips Last Lunch", 7, 20, "JM1", 24, "Joey", PORTLAND, ("Drive Misty For Me",)),
    Mission("Farewell 'Chunky' Lee Chong", 8, 21, "JM2", 25, "Joey", PORTLAND, ("Mike Lips Last Lunch",)),
    Mission("Van Heist", 9, 22, "JM3", 26, "Joey", PORTLAND, ("Farewell 'Chunky' Lee Chong",)),
    Mission("Cipriani's Chauffeur", 10, 23, "JM4", 27, "Joey", PORTLAND, ("Van Heist",)),
    Mission("Dead Skunk in the Trunk", 11, 24, "JM5", 28, "Joey", PORTLAND, ("Cipriani's Chauffeur", "Chaperone")),
    Mission("The Getaway", 12, 25, "JM6", 29, "Joey", PORTLAND, ("Dead Skunk in the Trunk", "Chaperone")),
    Mission("Taking Out the Laundry", 13, 26, "TM1", 30, "Toni", PORTLAND, ("Cipriani's Chauffeur",)),
    Mission("The Pick-Up", 14, 27, "TM2", 31, "Toni", PORTLAND, ("Taking Out the Laundry",)),
    Mission("Salvatore's Called a Meeting", 15, 28, "TM3", 32, "Toni", PORTLAND, ("The Pick-Up",)),
    Mission("Triads and Tribulations", 16, 29, "TM4", 33, "Toni", PORTLAND, ("Chaperone",)),
    Mission("Blow Fish", 17, 30, "TM5", 34, "Toni", PORTLAND, ("Triads and Tribulations",)),
    Mission("Chaperone", 18, 31, "FM1", 35, "Salvatore", PORTLAND, ("Salvatore's Called a Meeting",)),
    Mission("Cutting the Grass", 19, 32, "FM2", 36, "Salvatore", PORTLAND, ("Chaperone",)),
    Mission("Bomb Da Base: Act I", 20, 33, "FM21", 37, "Salvatore", PORTLAND, ("Cutting the Grass",)),
    Mission("Bomb Da Base: Act II", 21, 34, "FM3", 38, "Salvatore", PORTLAND, ("Bomb Da Base: Act I",)),
    Mission("Last Requests", 22, 35, "FM4", 39, "Salvatore", PORTLAND, ("Bomb Da Base: Act II",)),
    Mission("The Crook", 23, 36, "MEA1", 15, "Marty", PORTLAND, ("Drive Misty For Me",)),
    Mission("The Thieves", 24, 37, "MEA2", 16, "Marty", PORTLAND, ("The Crook",)),
    Mission("The Wife", 25, 38, "MEA3", 17, "Marty", PORTLAND, ("The Thieves",)),
    Mission("Her Lover", 26, 39, "MEA4", 18, "Marty", PORTLAND, ("The Wife",)),
    Mission("Turismo", 27, 40, "DIAB1", 40, "El Burro", PORTLAND, ("Van Heist",)),
    Mission("I Scream, You Scream", 28, 41, "DIAB2", 41, "El Burro", PORTLAND, ("Turismo",)),
    Mission("Trial By Fire", 29, 42, "DIAB3", 42, "El Burro", PORTLAND, ("I Scream, You Scream",)),
    Mission("Big'N'Veiny", 30, 43, "DIAB4", 43, "El Burro", PORTLAND, ("Trial By Fire",)),
    Mission("Sayonara Salvatore", 31, 44, "AM1", 44, "Asuka", STAUNTON, ("Last Requests",)),
    Mission("Under Surveillance", 32, 45, "AM2", 45, "Asuka", STAUNTON, ("Sayonara Salvatore",)),
    Mission("Paparazzi Purge", 33, 46, "AM3", 46, "Asuka", STAUNTON, ("Under Surveillance",)),
    Mission("Payday for Ray", 34, 47, "AM4", 47, "Asuka", STAUNTON, ("Paparazzi Purge",)),
    Mission("Two-Faced Tanner", 35, 48, "AM5", 48, "Asuka", STAUNTON, ("Payday for Ray",)),
    Mission("Bait", 36, 49, "AS1", 71, "Asuka", SHORESIDE, ("Grand Theft Aero",)),
    Mission("Espresso-2-Go!", 37, 50, "AS2", 72, "Asuka", SHORESIDE, ("Bait",)),
    Mission("S.A.M.", 38, 51, "AS3", 73, "Asuka", SHORESIDE, ("Espresso-2-Go!",)),
    Mission("Kanbu Bust-out", 39, 52, "KM1", 49, "Kenji", STAUNTON, ("Under Surveillance",)),
    Mission("Grand Theft Auto", 40, 53, "KM2", 50, "Kenji", STAUNTON, ("Kanbu Bust-out",)),
    Mission("Deal Steal", 41, 54, "KM3", 51, "Kenji", STAUNTON, ("Grand Theft Auto",)),
    Mission("Shima", 42, 55, "KM4", 52, "Kenji", STAUNTON, ("Deal Steal",)),
    Mission("Smack Down", 43, 56, "KM5", 53, "Kenji", STAUNTON, ("Shima",)),
    Mission("Silence the Sneak", 44, 57, "RM1", 54, "Ray", STAUNTON, ("Payday for Ray",)),
    Mission("Arms Shortage", 45, 58, "RM2", 55, "Ray", STAUNTON, ("Silence the Sneak",)),
    Mission("Evidence Dash", 46, 59, "RM3", 56, "Ray", STAUNTON, ("Arms Shortage",)),
    Mission("Gone Fishing", 47, 60, "RM4", 57, "Ray", STAUNTON, ("Evidence Dash",)),
    Mission("Plaster Blaster", 48, 61, "RM5", 58, "Ray", STAUNTON, ("Gone Fishing",)),
    Mission("Marked Man", 49, 62, "RM6", 59, "Ray", SHORESIDE, ("Plaster Blaster", "A Drop in the Ocean")),
    Mission("Liberator", 50, 63, "LOVE1", 60, "Donald Love", STAUNTON, ("Evidence Dash",)),
    Mission("Waka-Gashira Wipeout!", 51, 64, "LOVE2", 61, "Donald Love", STAUNTON, ("Liberator",)),
    Mission("A Drop in the Ocean", 52, 65, "LOVE3", 62, "Donald Love", STAUNTON, ("Waka-Gashira Wipeout!",)),
    Mission("Grand Theft Aero", 53, 66, "LOVE4", 67, "Donald Love", SHORESIDE, ("A Drop in the Ocean",)),
    Mission("Escort Service", 54, 67, "LOVE5", 68, "Donald Love", SHORESIDE, ("Grand Theft Aero",)),
    Mission("Decoy", 55, 68, "LOVE6", 69, "Donald Love", SHORESIDE, ("Escort Service",)),
    Mission("Love's Disappearance", 56, 69, "LOVE7", 70, "Donald Love", STAUNTON, ("Decoy",)),
    Mission("Bling-Bling Scramble", 57, 70, "YD1", 63, "King Courtney", STAUNTON, ("Sayonara Salvatore",)),
    Mission("Uzi Rider", 58, 71, "YD2", 64, "King Courtney", STAUNTON, ("Bling-Bling Scramble",)),
    Mission("Gangcar Round-Up", 59, 72, "YD3", 65, "King Courtney", STAUNTON, ("Uzi Rider",)),
    Mission("Kingdom Come", 60, 73, "YD4", 66, "King Courtney", STAUNTON, ("Gangcar Round-Up",)),
    Mission("Uzi Money", 61, 74, "HM_1", 74, "D-Ice", SHORESIDE, ("A Drop in the Ocean",)),
    Mission("Toyminator", 62, 75, "HM_2", 75, "D-Ice", SHORESIDE, ("Uzi Money",)),
    Mission("Rigged To Blow", 63, 76, "HM_3", 76, "D-Ice", SHORESIDE, ("Toyminator",)),
    Mission("Bullion Run", 64, 77, "HM_4", 77, "D-Ice", SHORESIDE, ("Rigged To Blow",)),
    Mission("Rumble", 65, 78, "HM_5", 78, "D-Ice", SHORESIDE, ("Bullion Run",)),
    Mission("The Exchange", 66, 79, "CAT2", 79, "Catalina", SHORESIDE, ("S.A.M.",)),
]

# The side missions with a pass of their own, started from a vehicle and open from the start on their island: no unlock and no contact.
# They give the logic more places to start from, as each story mission needs an item. Note RC3 and RC4: Rumpo Rampage registers RC4, Casino Calamity RC3.
SIDE_MISSIONS = [
    Mission("Diablo Destruction", 67, 0, "RC1", 3, "", PORTLAND),
    Mission("Mafia Massacre", 68, 0, "RC2", 4, "", PORTLAND),
    Mission("Rumpo Rampage", 69, 0, "RC4", 5, "", SHORESIDE),
    Mission("Casino Calamity", 70, 0, "RC3", 6, "", STAUNTON),
    Mission("Patriot Playground", 71, 0, "T4X4_1", 7, "", PORTLAND),
    Mission("A Ride in the Park", 72, 0, "T4X4_2", 8, "", STAUNTON),
    Mission("Gripped!", 73, 0, "T4X4_3", 9, "", STAUNTON),
    Mission("Multistorey Mayhem", 74, 0, "MM_1", 10, "", STAUNTON),
]

# Every location of the apworld: the story missions, then the side missions.
ALL_MISSIONS = MISSIONS + SIDE_MISSIONS

# The two missions whose passed routines open the next island in the game, and the event each one gives the logic.
ISLAND_OPENERS = {
    STAUNTON: "Last Requests",
    SHORESIDE: "A Drop in the Ocean",
}

# The last mission of the story: passing it wins the game.
GOAL_MISSION = "The Exchange"


def unlock_item_name(mission: Mission) -> str:
    """The free mode's unlock item of a mission."""
    return f"{mission.name} Unlock"


def contact_missions(contact: str) -> list[Mission]:
    """The missions a contact's progressive item unlocks, in order; Give Me Liberty is left out, as the intro starts it."""
    return [mission for mission in MISSIONS if mission.contact == contact and mission.unlock_id]
