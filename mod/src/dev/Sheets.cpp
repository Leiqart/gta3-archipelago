#include "Actions.h"
#include "Sheets.h"

using namespace Steps;

namespace {
    // Give Me Liberty dresses Claude back from the prison outfit the intro gave him (PLAYERP): later cutscenes crash on the prison model.
    void AfterGiveMeLiberty() {
        Actions::DressPlayer("PLAYER");
    }

    // The contacts' mission markers, walked into on foot (LOCATE_PLAYER_ON_FOOT_3D in each contact's trigger script of the main script).
    const CVector kLuigisMarker{892.75f, -425.75f, 13.875f};
    const CVector kJoeysMarker{1191.69f, -870.0f, 15.0f};
    const CVector kTonisMarker{1219.75f, -319.69f, 27.375f};

    // Bomb types of ARM_CAR_WITH_BOMB and IS_CAR_ARMED_WITH_BOMB: an ignition bomb fitted by 8-Ball's garage, then the same once the player arms it.
    constexpr int kIgnitionBomb = 2;
    constexpr int kIgnitionBombArmed = 5;
    // The timed bomb a bomb shop fits (Kanbu Bust-out), and the same once set off.
    constexpr int kTimedBomb = 1;
    constexpr int kTimedBombArmed = 4;

    // Give Me Liberty: the radar points of 8-Ball's hideout and of Luigi's club, and the corner where Misty waits (her char blip).
    const CVector kHideout{875.0f, -309.0f, 7.7f};
    const CVector kLuigisClub{906.19f, -426.0f, 13.8f};
    const CVector kMisty{1144.56f, -592.75f, 13.875f};

    // Don't Spank: the centre of the Pay 'n' Spray box (g248), and the mission garage (index in g276), whose radar point sits outside its door.
    const CVector kPayNSpray{924.85f, -361.3f, 10.0f};
    constexpr int kLockupGlobal = 276;
    // The raised marker in front of the garage, as the mission checks it (LOCATE_PLAYER_IN_CAR_3D, radius 2.5): the door opens for the car there.
    const CVector kLockupEntrance{1088.375f, -574.375f, 13.6875f};
    // From the player's own correction (2026-11-08): the car just inside the door, facing in (the garage's centre has no floor), and the player outside.
    const CVector kLockupParking{1081.40f, -574.12f, 14.57f};
    constexpr float kLockupParkingHeading = 87.0f;
    const CVector kOutsideLockup{1088.0f, -575.2f, 14.7f};

    // Drive Misty For Me: Misty comes out when the player honks while stopped within 3 m of this point; then the radar points at Joey's garage.
    const CVector kMistysDoor{937.875f, -259.75f, 3.9f};
    const CVector kJoeys{1196.0f, -874.0f, 13.8f};

    // Mike Lips Last Lunch: Forelli's car (g7016) is parked where the mission wants it back, so fitting and arming the bomb there is enough.
    constexpr int kForellisCarGlobal = 7016;
    // Outside the bistro's inner box (x up to 1350) but inside the outer one (x up to 1370), where the mission waits for the player; untested spot.
    const CVector kAwayFromBistro{1358.0f, -462.0f, 50.5f};

    // Van Heist: the security van (g7124) moves on at 600 health, when its guards flee; the lock up is mission garage g272, door on its south side.
    constexpr int kVanGlobal = 7124;
    constexpr int kVanLockupGlobal = 272;
    const CVector kVanLockup{1445.75f, -796.69f, 11.0f};
    // The mission's own marker in front of the door (LOCATE_PLAYER_IN_CAR_3D 1445, -811.5, radius 4 by 6), where it clears the wanted level.
    const CVector kVanLockupEntrance{1445.0f, -811.5f, 11.75f};
    // Untested guesses after the Don't Spank lesson: the van 6 m inside the door, facing north (heading 0), and the player 4 m outside the door.
    const CVector kVanLockupParking{1445.2f, -799.0f, 11.5f};
    constexpr float kVanLockupParkingHeading = 0.0f;
    const CVector kOutsideVanLockup{1445.2f, -809.5f, 11.75f};

    // Cipriani's Chauffeur: the radar points of the laundry and of Momma's, and the centres of the boxes the player must stop in there.
    const CVector kLaundry{843.0f, -660.0f, 14.5f};
    const CVector kLaundryStop{840.6f, -670.6f, 15.0f};
    const CVector kMommas{1216.75f, -327.56f, 25.5f};
    const CVector kMommasStop{1217.6f, -328.7f, 25.5f};

    // Dead Skunk in the Trunk: the car with the body (g7260), the crusher crane's radar point, and the box the mission wants the car stopped in.
    constexpr int kSkunkCarGlobal = 7260;
    const CVector kCrusher{1139.0f, 54.5f, 4.0f};
    const CVector kCrusherAreaCorner{1135.75f, 46.25f, -1.0f};
    const CVector kCrusherAreaOppositeCorner{1149.75f, 55.5f, 30.0f};
    // The centre of that box, searched for its ground from 8 m down; on the radar point the car never counted as stopped (2026-11-08 run).
    const CVector kCrusherPad{1142.75f, 50.9f, 6.0f};
    // The box slopes down to the south (0.5 m over 5 m): facing north the empty car rolled back 2 m and never counted as stopped, so it parks across the slope, facing west.
    constexpr float kCrusherPadHeading = 90.0f;
    const CVector kBesideCrusher{1142.75f, 40.0f, 4.0f};

    // The Getaway: the box at the St Mark's safehouse where the robbers are picked up and dropped off, and the box in front of the bank.
    const CVector kSafehouse{1086.0f, -227.0f, 10.0f};
    const CVector kSafehouseStop{1087.2f, -226.2f, 10.5f};
    const CVector kBank{1038.0f, -700.0f, 14.5f};
    const CVector kBankStop{1042.1f, -695.0f, 15.0f};

    // The Pick-Up: Toni's marker the mission checks on foot when the cash comes back.
    const CVector kTonisDoor{1219.56f, -320.69f, 27.375f};

    // Salvatore's Called a Meeting: the boxes at Luigi's club and at Momma's where the player stops and honks, and the radar point of Salvatore's garage.
    const CVector kLuigisStop{906.0f, -426.5f, 15.0f};
    const CVector kTonisStop{1199.9f, -324.25f, 25.5f};
    const CVector kSalvatoresGarage{1428.56f, -183.06f, 50.0f};
    // Mission garage g268 (1428.75..1442.5, -187..-179.875), door on its west side; it closes with the player still in the car (keep car, remain closed).
    constexpr int kSalvatoresGarageGlobal = 268;
    const CVector kSalvatoresEntrance{1424.5f, -183.4f, 50.5f};
    // Untested guess: 5 m inside the door, facing east (heading 270).
    const CVector kSalvatoresParking{1434.0f, -183.4f, 50.5f};
    constexpr float kSalvatoresParkingHeading = 270.0f;

    // The markers of Salvatore's mansion, of 8-Ball's shop in Harwood (Bomb Da Base: Act II) and of Asuka's condo in Staunton.
    const CVector kSalvatoresMarker{1455.69f, -187.25f, 55.56f};
    const CVector kBombShopMarker{1272.19f, -92.875f, 13.75f};
    const CVector kAsukasMarker{523.69f, -639.0f, 16.06f};

    // Chaperone: Maria (g552) rides the limo (g7984); the mission checks her stopped in it within 3 m of Chico, then in front of the party.
    constexpr int kMariaGlobal = 552;
    const CVector kChicoStop{775.5f, -557.25f, 14.0f};
    const CVector kParty{1256.56f, -1099.25f, 11.0f};
    const CVector kPartyStop{1259.0f, -1107.75f, 11.5f};

    // The Fuzz Ball: the box in front of the hall where the girls get out (996.75..1003.5, -883..-876.375), and the script's count of girls out of the car there.
    const CVector kFuzzBallStop{1000.1f, -879.7f, 14.5f};
    constexpr int kGirlsDroppedGlobal = 6844;

    // Cutting the Grass: in front of Luigi's club, inside the box the mission checks for a taxi (905..910, -432..-419), and the harbour meeting point.
    const CVector kLuigisClubFront{906.5f, -425.0f, 13.8f};
    const CVector kHarbourMeeting{1529.0f, -827.0f, 10.0f};

    // Blow Fish: the dustcart (g7880) with its timed bomb, the box between the gas canisters, and an untested spot away from it.
    constexpr int kDustcartGlobal = 7880;
    const CVector kFishFactory{965.0f, -1117.0f, 13.5f};
    const CVector kFishFactoryCorner{961.0f, -1122.75f, 12.5f};
    const CVector kFishFactoryOppositeCorner{969.5f, -1112.5f, 15.0f};
    const CVector kFishFactoryPark{965.25f, -1117.6f, 14.0f};
    const CVector kAwayFromFishFactory{965.0f, -1090.0f, 15.0f};

    // Bomb Da Base: Act II: 8-Ball costs $100000; the boat's entry box, the sniper's vantage point and the twelve Cartel guards (g8532 to g8576).
    const CVector kBoatEntryStop{1529.0f, -824.0f, 11.5f};
    const CVector kVantage{1498.69f, -870.5f, 26.75f};
    const std::vector<int> kCartelGuards{8532, 8536, 8540, 8544, 8548, 8552, 8556, 8560, 8564, 8568, 8572, 8576};

    // Last Requests: the docks where Maria waits, her boat (g8940), Asuka (g576), and the dock in Staunton both must reach on the boat.
    const CVector kDocks{827.25f, -1084.25f, 6.75f};
    constexpr int kMariasBoatGlobal = 8940;
    constexpr int kAsukaGlobal = 576;
    const CVector kStauntonDock{570.25f, -683.4f, 0.0f};

    // Sayonara Salvatore: he leaves Luigi's club at the hour and minute in g9804 and g9808; the player waits in Portland, out of the Red Light District.
    constexpr int kLeavingHourGlobal = 9804;
    constexpr int kLeavingMinuteGlobal = 9808;
    const CVector kPortlandWait{1196.0f, -874.0f, 13.8f};

    // Staunton's mission markers: Kenji's casino and Ray's toilets in Belleville Park.
    const CVector kKenjisMarker{459.06f, -1413.0f, 26.06f};
    const CVector kRaysMarker{38.75f, -725.375f, 22.75f};

    // Paparazzi Purge: the reporter's boat (g10456); sinking it passes the mission.
    constexpr int kReporterBoatGlobal = 10456;

    // Payday for Ray: the four pay phones, where the player answers on foot, then Ray's toilets, where the next phone sends him.
    const CVector kPhone1{229.19f, -1537.56f, 26.0f};
    const CVector kPhone2{-53.5f, -699.06f, 26.0f};
    const CVector kPhone3{204.75f, -135.0f, 16.0f};
    const CVector kPhone4{60.25f, -964.75f, 26.06f};

    // Two-Faced Tanner: the box where the player watches Tanner's car, and Tanner himself (g10548, created when he walks to it); his death passes.
    const CVector kTannerWatch{414.4f, -1378.6f, 26.5f};
    constexpr int kTannerGlobal = 10548;

    // Kanbu Bust-out: the stolen police car (g10568), the police station yard and an untested spot 25 m away, the 'police station blown' flag, the dojo.
    constexpr int kPoliceCarGlobal = 10568;
    const CVector kPoliceStation{327.0f, -1086.0f, 25.0f};
    const CVector kPoliceStationYard{327.69f, -1084.6f, 25.0f};
    const CVector kAwayFromStation{327.7f, -1110.0f, 25.0f};
    constexpr int kStationBlownGlobal = 3292;
    const CVector kDojo{105.56f, -1292.63f, 26.0f};

    // Grand Theft Auto: the three cars (g10756, g10764, g10768), mission garage g316 (377.25..383.69, -511.375..-502, door on its west side) and the count of cars delivered.
    constexpr int kImportGarageGlobal = 316;
    constexpr int kDeliveredGlobal = 10844;
    const CVector kImportEntrance{373.5f, -506.7f, 26.0f};
    // Untested guess: the middle of the short box, facing east (heading 270), and the player 5 m outside the door.
    const CVector kImportParking{380.47f, -506.7f, 26.0f};
    constexpr float kImportParkingHeading = 270.0f;
    const CVector kImportOutside{372.5f, -506.7f, 26.0f};

    // Deal Steal: the hospital car park in Rockford, the count of Colombians killed (4) and of their cars wrecked (2), and Kenji's casino where the briefcase goes.
    const CVector kHospitalLot{231.06f, -26.25f, 16.0f};
    constexpr int kColombiansKilledGlobal = 11032;
    constexpr int kCartelCarsWreckedGlobal = 11036;
    const CVector kCasino{452.25f, -1465.75f, 17.56f};

    // Shima: the second briefcase, the third business, and the yard of the gang that robbed it (Hepburn Heights, Portland).
    const CVector kSecondBriefcase{119.5f, -1110.5f, 25.6f};
    const CVector kThirdBusiness{-91.5f, -484.19f, 15.19f};
    const CVector kGangYard{938.0f, -185.0f, 4.5f};

    // Silence the Sneak: the box the player stops in to check the house out (its centre), and the window box a projectile must land in.
    const CVector kWitSecStop{353.1f, -451.2f, 22.0f};
    const CVector kWitSecWindow{378.3f, -443.2f, 29.9f};
}

const std::vector<Sheets::Sheet>& Sheets::Run() {
    // Built on first use, from the game thread, rather than while the DLL loads.
    static const std::vector<Sheet> run = {
        {.script = "eight", .passedFlag = 980, .afterSkip = AfterGiveMeLiberty, .steps = {
            {"get in the car", InCar, EnterBlipCar},
            {"wait for 8-Ball to get in", BlipNear(kHideout), JustWait},
            {"drive to 8-Ball's hideout", BlipNear(kLuigisClub), GoToBlip, 60},
            {"drive to Luigi's club", BlipNear(kMisty), GoToBlip, 60},
            {"pick up Misty", BlipNear(kLuigisClub), PickUpFriend},
            {"bring Misty to Luigi's", UntilMissionEnds, GoToBlip},
        }},
        {.script = "luigi2", .passedFlag = 996, .start = kLuigisMarker, .steps = {
            {"kill the dealer", BlipIs(BLIP_CAR), KillMarked},
            {"get in the dealer's car", [] { return InCar() && NoBlip(); }, EnterBlipCar},
            {"respray the car at the Pay 'n' Spray", BlipNear(kLockupEntrance), GoTo(kPayNSpray)},
            {"open the garage door", GarageOpen(kLockupGlobal), GoTo(kLockupEntrance)},
            {"park inside the garage", CarInGarage(kLockupGlobal), ParkAt(kLockupParking, kLockupParkingHeading)},
            {"get out of the garage", UntilMissionEnds, GetOutTo(kOutsideLockup)},
        }},
        {.script = "luigi3", .passedFlag = 1000, .start = kLuigisMarker, .steps = {
            {"get a car", InCar, GetCar},
            {"drive to Misty's door", PlayerNear(kMistysDoor, 3.0f), GoToBlip},
            {"honk until Misty gets in", BlipNear(kJoeys), Honk},
            {"take Misty to Joey's garage", UntilMissionEnds, GoToBlip, 60},
        }},
        // Joey only gives Mike Lips Last Lunch between 5:00 and 21:00 (GET_TIME_OF_DAY in his trigger).
        {.script = "joey1", .passedFlag = 1016, .start = kJoeysMarker, .beforeStart = SetClock(8), .steps = {
            {"get in Forelli's car", InCar, EnterBlipCar},
            {"fit an ignition bomb", CarArmedWith(kForellisCarGlobal, kIgnitionBomb), ArmBomb(kForellisCarGlobal, kIgnitionBomb)},
            {"get out of the way", OnFoot, GetOutTo(kAwayFromBistro)},
            {"arm the bomb and watch Lips drive off", UntilMissionEnds, ArmBomb(kForellisCarGlobal, kIgnitionBombArmed), 60},
        }},
        // Chunky's death passes the mission at any point, even before the fight.
        {.script = "joey2", .passedFlag = 1020, .start = kJoeysMarker, .steps = {
            {"kill Chunky", UntilMissionEnds, KillMarked},
        }},
        {.script = "joey3", .passedFlag = 1024, .start = kJoeysMarker, .steps = {
            {"damage the van until its guards give up", CarHealthAtMost(kVanGlobal, 600), SetCarHealth(kVanGlobal, 590)},
            {"wait for the guards to run away", CarEmpty(kVanGlobal), ClearWanted},
            {"get in the van", BlipNear(kVanLockup), EnterBlipCar},
            {"open the lock up door", GarageOpen(kVanLockupGlobal), GoTo(kVanLockupEntrance)},
            {"park inside the lock up", CarInGarage(kVanLockupGlobal), ParkAt(kVanLockupParking, kVanLockupParkingHeading)},
            {"get out of the lock up", UntilMissionEnds, GetOutTo(kOutsideVanLockup)},
        }},
        {.script = "joey4", .passedFlag = 1028, .start = kJoeysMarker, .steps = {
            {"wait for Toni and the drive out of the garage", BlipNear(kLaundry), JustWait, 60},
            {"stop at the laundry and wait out the ambush", BlipNear(kMommas), StopAt(kLaundryStop), 90},
            {"take Toni to Momma's", UntilMissionEnds, StopAt(kMommasStop), 60},
        }},
        {.script = "joey5", .passedFlag = 1032, .start = kJoeysMarker, .steps = {
            {"get in the car with the body", BlipNear(kCrusher), EnterBlipCar},
            {"park under the crusher crane", CarStoppedIn(kSkunkCarGlobal, kCrusherAreaCorner, kCrusherAreaOppositeCorner), ParkAt(kCrusherPad, kCrusherPadHeading)},
            // The crane only lifts a car nobody sits in; the mission removes its blips once the crane lifts the car.
            {"get out and let the crane lift it", NoBlip, GetOutTo(kBesideCrusher), 60},
            {"wait for the crusher", UntilMissionEnds, JustWait, 60},
        }},
        // Joey only gives The Getaway between 6:00 and 14:00, and the mission fails at 17:00: 8:00 leaves 9 game hours, 9 real minutes.
        {.script = "joey6", .passedFlag = 1036, .start = kJoeysMarker, .beforeStart = SetClock(8), .steps = {
            {"get a car with 3 free seats", InCar, GetCar},
            {"stop at the safehouse and honk for the robbers", BlipNear(kBank), Both(StopAt(kSafehouseStop), Honk), 60},
            {"stop at the bank and wait for the robbery", BlipNear(kSafehouse), StopAt(kBankStop), 120},
            {"take the robbers back to the safehouse", UntilMissionEnds, StopAt(kSafehouseStop), 60},
        }},
        // Toni's missions open with Cipriani's Chauffeur but come after Joey's: passing Salvatore's Called a Meeting puts Joey 5 and 6 on hold until Chaperone.
        {.script = "toni1", .passedFlag = 1044, .start = kTonisMarker, .steps = {
            {"blow up the laundry vans", UntilMissionEnds, DestroyMarkedCars},
        }},
        {.script = "toni2", .passedFlag = 1048, .start = kTonisMarker, .steps = {
            {"pick up the cash", BlipIs(BLIP_CHAR), CollectPickup, 60},
            {"waste the Triads", BlipIs(BLIP_COORD), KillMarked},
            {"bring the cash to Toni's", UntilMissionEnds, WalkTo(kTonisDoor), 60},
        }},
        {.script = "toni3", .passedFlag = 1052, .start = kTonisMarker, .steps = {
            {"walk into Joey's garage", InCar, WalkTo(kJoeysMarker)},
            {"wait for Joey and the drive out", BlipNear(kLuigisClub), JustWait, 60},
            {"honk for Luigi at his club", BlipNear(kTonisStop), Both(StopAt(kLuigisStop), Honk), 60},
            {"honk for Toni at Momma's", BlipNear(kSalvatoresGarage), Both(StopAt(kTonisStop), Honk), 60},
            {"open Salvatore's garage", GarageOpen(kSalvatoresGarageGlobal), GoTo(kSalvatoresEntrance)},
            {"park inside Salvatore's garage", UntilMissionEnds, ParkAt(kSalvatoresParking, kSalvatoresParkingHeading), 60},
        }},
        // Chaperone restarts the four missions that Salvatore's Called a Meeting put on hold (Luigi 4 and 5, Joey 5 and 6).
        {.script = "frank1", .passedFlag = 1068, .start = kSalvatoresMarker, .steps = {
            {"wait for Maria and the drive out of the garage", BlipNear(kChicoStop), JustWait, 60},
            {"stop beside Chico at the rail station", BlipNear(kParty), StopAt(kChicoStop), 90},
            {"drop Maria in front of the party", CharOnFoot(kMariaGlobal), StopAt(kPartyStop)},
            {"wait out the police raid", BlipIs(BLIP_CHAR), JustWait, 180},
            // She runs to the player by herself and joins at 4 m: teleporting the limo beside her crushed her (2026-11-08 run).
            {"wait for Maria to run back and get in", BlipNear(kSalvatoresGarage), ClearWanted, 60},
            {"open Salvatore's garage", GarageOpen(kSalvatoresGarageGlobal), ParkAt(kSalvatoresEntrance, kSalvatoresParkingHeading)},
            {"park the limo inside", UntilMissionEnds, ParkAt(kSalvatoresParking, kSalvatoresParkingHeading), 120},
        }},
        {.script = "luigi4", .passedFlag = 1004, .start = kLuigisMarker, .steps = {
            {"kill the two pimps in their car", UntilMissionEnds, KillMarkedCarOccupants, 60},
        }},
        // A Kuruma takes 3 girls: 3, 3 then 2 bring all 8, which passes at once instead of waiting for the 5-minute timer.
        {.script = "luigi5", .passedFlag = 1008, .start = kLuigisMarker, .steps = {
            {"get a car with 3 free seats", InCar, GetCar},
            {"pick up three girls", PassengersAtLeast(3), PickUpNearestFriend, 90},
            {"drop them at the Fuzz Ball", GlobalAtLeast(kGirlsDroppedGlobal, 3), StopAt(kFuzzBallStop), 60},
            {"pick up three more girls", PassengersAtLeast(3), PickUpNearestFriend, 90},
            {"drop them at the Fuzz Ball", GlobalAtLeast(kGirlsDroppedGlobal, 6), StopAt(kFuzzBallStop), 60},
            {"pick up the last two girls", PassengersAtLeast(2), PickUpNearestFriend, 90},
            {"drop the last two at the Fuzz Ball", UntilMissionEnds, StopAt(kFuzzBallStop), 60},
        }},
        {.script = "toni4", .passedFlag = 1056, .start = kTonisMarker, .steps = {
            {"kill the three Triad warlords", UntilMissionEnds, KillMarked, 60},
        }},
        // Cutting the Grass sets g1088, which keeps Salvatore's next mission shut until Blow Fish clears it: Toni 5 comes between them.
        {.script = "frank2", .passedFlag = 1072, .start = kSalvatoresMarker, .steps = {
            {"get a taxi", InCar, GetTaxi},
            {"wait in front of Luigi's club for Curly Bob", BlipNear(kHarbourMeeting), StopAt(kLuigisClubFront), 150},
            {"drive Curly Bob to the harbour meeting", BlipIs(BLIP_CHAR), StopAt(kHarbourMeeting), 150},
            {"whack Curly Bob", UntilMissionEnds, KillMarked},
        }},
        {.script = "toni5", .passedFlag = 1060, .start = kTonisMarker, .steps = {
            {"get in the dustcart", BlipNear(kFishFactory), EnterBlipCar},
            {"park between the gas canisters", CarStoppedIn(kDustcartGlobal, kFishFactoryCorner, kFishFactoryOppositeCorner), ParkAt(kFishFactoryPark, 0.0f)},
            {"get away from the dustcart", OnFoot, GetOutTo(kAwayFromFishFactory)},
            {"blow up the dustcart", UntilMissionEnds, ExplodeScriptCar(kDustcartGlobal), 60},
        }},
        // Bomb Da Base: Act I is a cutscene that registers its pass.
        {.script = "frank21", .passedFlag = 1076, .start = kSalvatoresMarker, .steps = {
            {"watch the cutscene", UntilMissionEnds, JustWait, 60},
        }},
        {.script = "frank3", .passedFlag = 1080, .start = kBombShopMarker, .beforeStart = EnsureMoney(100000), .steps = {
            {"get a car for 8-Ball", PassengersAtLeast(1), GetCar, 120},
            {"drive 8-Ball to the boat", BlipNear(kVantage), StopAt(kBoatEntryStop), 60},
            {"climb to the vantage point", [] { return OnFoot() && NoBlip(); }, WalkTo(kVantage)},
            {"pick off the guards while 8-Ball plants the bomb", UntilMissionEnds, KillScriptChars(kCartelGuards), 240},
        }},
        // Last Requests: Salvatore's car carries an armed ignition bomb; the mission only wants the player out of the Red Light District, then at the docks.
        {.script = "frank4", .passedFlag = 1084, .start = kSalvatoresMarker, .steps = {
            {"go to the docks, leaving the trapped car alone", BlipNear(kStauntonDock), WalkTo(kDocks), 120},
            {"board Maria's boat", InCar, BoardScriptCar(kMariasBoatGlobal)},
            {"sail to Staunton with Maria and Asuka on deck", UntilMissionEnds, SailTo(kStauntonDock, {kMariaGlobal, kAsukaGlobal}), 120},
        }},
        // Sayonara Salvatore: he leaves Luigi's three game hours after the start; the clock jumps to 2 minutes before.
        {.script = "asuka1", .passedFlag = 1264, .start = kAsukasMarker, .steps = {
            {"wait in Portland, out of the Red Light District", BlipIs(BLIP_CHAR), Both(WalkTo(kPortlandWait), ClockToScriptTime(kLeavingHourGlobal, kLeavingMinuteGlobal, 2)), 120},
            {"kill Salvatore", UntilMissionEnds, KillMarked, 60},
        }},
        // Staunton. Asuka's Under Surveillance opens Kenji; both Kenji's triggers and Asuka 5's end for good once Donald Love 2 and 4 are passed.
        {.script = "asuka2", .passedFlag = 1268, .start = kAsukasMarker, .steps = {
            {"kill the ten spies", UntilMissionEnds, KillMarked, 60},
        }},
        {.script = "asuka3", .passedFlag = 1272, .start = kAsukasMarker, .steps = {
            {"sink the reporter's boat", UntilMissionEnds, ExplodeScriptCar(kReporterBoatGlobal), 60},
        }},
        // Payday for Ray ends at Ray's toilets, on the very marker of Silence the Sneak, which then starts by itself: Ray 1 must come next.
        {.script = "asuka4", .passedFlag = 1276, .start = kAsukasMarker, .steps = {
            {"answer the first pay phone", AnyBlipNear(kPhone2), WalkTo(kPhone1), 60},
            {"answer the second pay phone", AnyBlipNear(kPhone3), WalkTo(kPhone2), 60},
            {"answer the third pay phone", AnyBlipNear(kPhone4), WalkTo(kPhone3), 60},
            {"answer the last pay phone", BlipNear(kRaysMarker), WalkTo(kPhone4), 60},
            {"meet Ray at the toilets", UntilMissionEnds, WalkTo(kRaysMarker), 60},
        }},
        {.script = "ray1", .passedFlag = 1288, .start = kRaysMarker, .steps = {
            {"torch the witness protection house", BlipIs(BLIP_CHAR), Both(StopAt(kWitSecStop), ThrowGrenadeInto(kWitSecWindow)), 90},
            {"kill McAffrey", UntilMissionEnds, KillMarked, 90},
        }},
        {.script = "asuka5", .passedFlag = 1280, .start = kAsukasMarker, .steps = {
            {"watch Tanner's car", BlipIs(BLIP_CAR), StopAt(kTannerWatch)},
            {"kill Tanner", UntilMissionEnds, KillScriptChars({kTannerGlobal}), 90},
        }},
        {.script = "kenji1", .passedFlag = 1316, .start = kKenjisMarker, .steps = {
            {"steal a police car", InModel(MODEL_POLICE), GetCarOf(MODEL_POLICE)},
            {"fit a timed bomb", BlipNear(kPoliceStation), ArmBomb(kPoliceCarGlobal, kTimedBomb)},
            {"drive into the police station yard", PlayerNear(kPoliceStationYard, 2.0f), GoTo(kPoliceStationYard)},
            // The mission only sees the bomb set off while the player still sits in the police car in the yard.
            {"set the bomb off", CarArmedWith(kPoliceCarGlobal, kTimedBombArmed), ActivateTimedBomb(kPoliceCarGlobal)},
            {"get out of the way", GlobalAtLeast(kStationBlownGlobal, 1), GetOutTo(kAwayFromStation)},
            {"pick up the Kanbu", PassengersAtLeast(1), Both(ClearWanted, GetCar), 60},
            {"lose the cops", BlipNear(kDojo), ClearWanted},
            {"drive the Kanbu to the dojo", UntilMissionEnds, StopAt(kDojo), 60},
        }},
        {.script = "kenji2", .passedFlag = 1320, .start = kKenjisMarker, .steps = {
            {"get in the first car", InScriptCar(10756), EnterScriptCar(10756)},
            {"open the garage door", GarageOpen(kImportGarageGlobal), GoTo(kImportEntrance)},
            {"park the first car inside", CarInGarage(kImportGarageGlobal), ParkAt(kImportParking, kImportParkingHeading)},
            {"get out and walk outside", GlobalAtLeast(kDeliveredGlobal, 1), GetOutTo(kImportOutside)},
            {"get in the second car", InScriptCar(10764), EnterScriptCar(10764)},
            {"open the garage door", GarageOpen(kImportGarageGlobal), GoTo(kImportEntrance)},
            {"park the second car inside", CarInGarage(kImportGarageGlobal), ParkAt(kImportParking, kImportParkingHeading)},
            {"get out and walk outside", GlobalAtLeast(kDeliveredGlobal, 2), GetOutTo(kImportOutside)},
            {"get in the third car", InScriptCar(10768), EnterScriptCar(10768)},
            {"open the garage door", GarageOpen(kImportGarageGlobal), GoTo(kImportEntrance)},
            {"park the third car inside", CarInGarage(kImportGarageGlobal), ParkAt(kImportParking, kImportParkingHeading)},
            {"get out and walk outside", UntilMissionEnds, GetOutTo(kImportOutside), 60},
        }},
        {.script = "kenji3", .passedFlag = 1324, .start = kKenjisMarker, .steps = {
            {"steal a Yardie car", InModel(MODEL_YARDIE), GetCarOf(MODEL_YARDIE)},
            {"pick up Kenji's man", BlipNear(kHospitalLot), PickUpFriend, 60},
            {"honk at the hospital car park to start the deal", Marked(BLIP_CAR), Both(StopAt(kHospitalLot), Honk), 90},
            {"kill the Colombians and wreck their cars", AllOf(GlobalAtLeast(kColombiansKilledGlobal, 4), GlobalAtLeast(kCartelCarsWreckedGlobal, 2)), Both(KillMarked, DestroyMarkedCars), 60},
            {"recover the briefcase", BlipNear(kCasino), CollectPickup},
            {"take it to the casino", UntilMissionEnds, StopAt(kCasino), 60},
        }},
        {.script = "kenji4", .passedFlag = 1328, .start = kKenjisMarker, .steps = {
            {"collect the first briefcase", AnyBlipNear(kSecondBriefcase), CollectPickup},
            {"collect the second briefcase", BlipNear(kThirdBusiness), CollectPickup},
            {"go to the third business", AnyBlipNear(kGangYard), StopAt(kThirdBusiness), 90},
            {"punish the gang and take the money", BlipNear(kCasino), Both(KillMarked, CollectPickup), 60},
            {"take the money back to the casino", UntilMissionEnds, StopAt(kCasino), 60},
        }},
        // Smack Down: each dealer only appears when the player comes within 90 m of his spot; the mission wants at least 8 of the 15.
        {.script = "kenji5", .passedFlag = 1332, .start = kKenjisMarker, .steps = {
            {"run down the Yardie dealers", UntilMissionEnds, HuntMarked, 300},
        }},
    };
    return run;
}

const Sheets::Sheet* Sheets::Find(const std::string& script) {
    for (const Sheet& sheet : Run()) {
        if (script == sheet.script) return &sheet;
    }
    return nullptr;
}
