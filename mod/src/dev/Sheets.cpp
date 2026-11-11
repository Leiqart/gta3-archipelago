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
    const CVector kDocksBlip{835.69f, -1091.69f, 6.0f};
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

    // Kanbu Bust-out: the stolen police car (g10568), the police station yard and a spot 18 m away, the 'police station blown' flag, the dojo.
    constexpr int kPoliceCarGlobal = 10568;
    const CVector kPoliceStation{327.0f, -1086.0f, 25.0f};
    const CVector kPoliceStationYard{327.69f, -1084.6f, 25.0f};
    // North of the yard (the station and the Kanbu's cell lie south of it, IS_EXPLOSION_IN_AREA y -1094..-1072), out of the blast, and within 30 m of the Kanbu, who leaves a leader further away.
    const CVector kAwayFromStation{327.7f, -1066.0f, 25.0f};
    constexpr int kStationBlownGlobal = 3292;
    const CVector kDojo{105.56f, -1292.63f, 26.0f};

    // Smack Down: the dealers killed.
    constexpr int kDealersKilledGlobal = 11376;

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

    // Donald Love's marker at his building in Bedford Point.
    const CVector kLovesMarker{87.25f, -1548.56f, 28.25f};
    // The Colombian gang car Love's missions ask to steal (model 138, the Cartel Cruiser).
    constexpr int kCartelCar = 138;

    // Arms Shortage: the count of Cartel killed, and a spot 1.5 m north of Phil (136.5, 176.69), the 2 m he must be checked on from.
    constexpr int kCartelKilledGlobal = 12236;
    const CVector kBesidePhil{136.5f, 178.2f, 11.5f};

    // Evidence Dash: the van with the photos (g12404) drops one each time its health falls fast enough, the count of photos picked up (g12476), and the car the player leaves them in (g12424).
    constexpr int kEvidenceVanGlobal = 12404;
    constexpr int kPhotosCollectedGlobal = 12476;
    constexpr int kPhotosCarGlobal = 12424;

    // Gone Fishing: the police boat to steal (g12596), the partner's boat (g12588) and the partner (g12580).
    constexpr int kPoliceBoatGlobal = 12596;
    constexpr int kPartnersBoatGlobal = 12588;
    constexpr int kPartnerGlobal = 12580;

    // Plaster Blaster: the ambulance carrying the witness (g12972), and its phase (g12980), 1 once it has spotted the player and heads for the hospital.
    constexpr int kAmbulanceGlobal = 12972;
    constexpr int kAmbulanceAlertedGlobal = 12980;

    // Grand Theft Aero: the lift up the tower at the Panlantic construction site (its roof, 1.5 m above, is where a ground search lands), and the gang guarding the site.
    const CVector kPanlanticLift{366.94f, -328.0f, 20.25f};
    const std::vector<int> kPanlanticGang{14540, 14544, 14548, 14552, 14556, 14560, 14564, 14568, 14572, 14576};

    // Marked Man: the box at the airport where Ray must stop.
    const CVector kAirportStop{-738.25f, -582.875f, 10.0f};
    // Marked Man: Ray's lock-up, where his stash waits, and its door.
    const CVector kRaysLockup{241.13f, -997.75f, 20.94f};
    const CVector kRaysLockupDoor{230.19f, -996.44f, 21.0f};

    // Liberator: the gate of Love's building, which only opens for a Colombian gang car.
    const CVector kLovesGate{49.06f, -1550.5f, 26.0f};
    // Liberator: the old gentleman held in the hideout (g13276), and a spot outside its gate (IS_PLAYER_IN_AREA_3D x 92..96, y -329..-316), untested.
    constexpr int kHostageGlobal = 13276;
    const CVector kOutsideHideout{101.0f, -322.0f, 15.0f};

    // Waka-Gashira Wipeout!: the top of the multi-storey car park in Newport, Kenji (g580), and the mission's phase (g13524), 1 once he is dead.
    const CVector kMultiStorey{305.0f, -545.0f, 30.0f};
    constexpr int kKenjiGlobal = 580;
    constexpr int kKenjiWhackedGlobal = 13524;

    // A Drop in the Ocean: the boat waiting at the harbour (g13648).
    constexpr int kPackagesBoatGlobal = 13648;

    // Escort Service: the truck (g14884), the script's phase (g14900), and the route it drives the truck along with the phase from which it sends it to each point, at the script's heights (the tunnel runs at -21 m).
    // At the tunnel's exit (phase 3), the script waits for Shoreside's collision, so for the player to scout ahead, before phase 4 sends the truck on.
    // The garage only opens once the truck is in the approach box east of it (phase 9), the last point is inside it (IS_CAR_STOPPED_IN_AREA_2D), and it only closes once the player is out of the box.
    constexpr int kTruckGlobal = 14884;
    constexpr int kTruckPhaseGlobal = 14900;
    const std::vector<Steps::RoutePoint> kTruckRoute{
        {0, {173.44f, 76.25f, 14.94f}}, {1, {302.0f, -12.375f, 15.0f}}, {1, {559.5f, 88.75f, -21.06f}}, {2, {-320.375f, 87.125f, -21.06f}},
        {3, {-573.81f, -426.375f, -5.75f}}, {5, {-634.94f, -491.19f, 16.13f}}, {6, {-864.44f, -282.44f, 32.5f}}, {7, {-975.13f, -74.5f, 36.94f}}, {8, {-1030.0f, -73.4f, 37.81f}}, {9, {-1043.0f, -73.375f, 37.81f}}};
    const CVector kTunnelExitScout{-634.94f, -491.19f, 16.13f};
    // The truck's spot at Love's building (83.56, -1592.25, facing south), and a guessed spot 12 m south of it, facing north.
    const CVector kTruckStart{83.56f, -1592.25f, 25.06f};
    const CVector kTruckStartCorner{78.5f, -1597.0f, 20.0f};
    const CVector kTruckStartOppositeCorner{88.5f, -1587.0f, 32.0f};
    const CVector kFacingTheTruck{83.56f, -1604.25f, 25.06f};
    const CVector kShoresideGarageCorner{-1049.125f, -77.44f, 30.0f};
    const CVector kShoresideGarageOppositeCorner{-1037.19f, -69.13f, 45.0f};
    const CVector kOutsideShoresideGarage{-1025.0f, -73.4f, 37.8f};

    // Asuka's marker for her last missions, and Catalina's at the Cochrane Dam for The Exchange.
    const CVector kAsukasLastMarker{367.25f, -328.06f, 19.5f};
    const CVector kExchangeMarker{-362.75f, 246.5f, 60.0f};
    // The Exchange: Catalina's helicopter, the middles of the two areas that bring out the next gangs (x -1124..-927, y 391..485; x -1221..-1111, y 366..446, z 20..30), and all her men.
    constexpr int kCatalinasHeliGlobal = 17192;
    // The small dam (zone WEE_DAM, which the mission waits for between the dam and the first area), where it switches the ped roads off (x -942..-953, y 337..361); untested guess.
    const CVector kSmallDam{-948.0f, 349.0f, 20.0f};
    const CVector kCatalinasFirstArea{-1025.0f, 438.0f, 25.0f};
    const CVector kCatalinasHideout{-1166.0f, 406.0f, 25.0f};
    const std::vector<int> kCatalinasGang{17056, 17060, 17064, 17068, 17072, 17076, 17080, 17084, 17088, 17092, 17096, 17100, 17104,
        17108, 17112, 17116, 17120, 17124, 17128, 17132, 17136, 17140, 17144, 17148, 17152, 17156};

    // Decoy: the decoy Securicar (g15116) is created when the player comes within 200 m of the warehouse; the mission wants it 160 m away or more after 3 minutes.
    constexpr int kDecoyGlobal = 15116;
    const CVector kDecoyApproach{-1010.0f, -73.5f, 39.0f};
    const CVector kFarFromWarehouse{-738.25f, -582.875f, 10.0f};
    const CVector kAlsoFarFromWarehouse{-864.44f, -282.44f, 32.5f};

    // Bait: a death squad counts as trapped when its car, still whole, comes within 17 x 30 m of this point at Pike Creek.
    const CVector kPikeCreekTrap{-1185.0f, 105.0f, 70.0f};

    // Espresso-2-Go: the 12 stall spots the script sets (g16164..g16256), 9 used; a stall shows up once the player is within 150 m of it or in its district (heights unknown: the ground is searched from 22 m).
    const std::vector<CVector> kEspressoStalls{
        {1342.0f, -821.0f, 20.0f}, {1024.06f, -465.56f, 20.0f}, {1351.19f, -259.5f, 20.0f}, {286.38f, -667.75f, 20.0f},
        {28.0f, -850.0f, 20.0f}, {14.13f, -1140.0f, 20.0f}, {73.13f, -1359.75f, 20.0f}, {-221.0f, -197.0f, 20.0f},
        {282.75f, -1492.38f, 20.0f}, {-644.0f, -721.5f, 20.0f}, {-212.0f, 310.0f, 20.0f}, {-1255.0f, -113.0f, 20.0f}};

    // S.A.M.: the boat (g16288), the flag set once the cargo falls (g16568), the count collected (g16564), a spot 8 m from the runway marker buoy, and the countdown to the plane.
    constexpr int kSamBoatGlobal = 16288;
    constexpr int kCargoFallenGlobal = 16568;
    constexpr int kCargoCollectedGlobal = 16564;
    const CVector kBesideRunwayBuoy{-797.0f, -1310.0f, 0.0f};
    constexpr int kPlaneCountdownGlobal = 16284;

    // The four phone contacts: their trigger waits for the player stopped on foot at the ringing phone (LOCATE_STOPPED_PLAYER_ON_FOOT_3D).
    const CVector kMartysPhone{1224.56f, -840.25f, 15.0f};
    const CVector kElBurrosPhone{938.375f, -230.5f, 3.875f};
    const CVector kCourtneysPhone{120.69f, -272.06f, 16.06f};
    const CVector kDIcesPhone{-443.5f, -6.06f, 3.75f};

    // Marty Chonks: where the passenger must be stopped in the car at his meat factory (4 x 4 x 6), the back yard of Her Lover, and the factory gate.
    const CVector kMartysYard{1205.69f, -789.19f, 14.75f};
    const CVector kMartysBack{1217.0f, -794.0f, 13.88f};
    const CVector kMartysFactory{1195.56f, -805.0f, 13.69f};
    // The Wife: a spot 5 m west of Mrs. Chonks; Her Lover: a spot 6 m south of Carlos, untested.
    const CVector kWestOfMrsChonks{1059.0f, -378.0f, 13.875f};
    const CVector kSouthOfCarlos{897.0f, -482.0f, 14.5f};
    // The Wife: the sea by the docks, where the police boats wait (837, -1115.6 floats at -0.19).
    const CVector kHarbourSea{837.0f, -1130.0f, 1.0f};

    // Turismo: the starting grid; I Scream, You Scream: the box at Atlantic Quays where the van parks, the gang members it lures (g9572 to g9584).
    const CVector kTurismoGrid{1042.94f, -858.69f, 13.75f};
    // I Scream, You Scream: the spot of El Burro's bomb in Harwood.
    const CVector kElBurrosBomb{934.875f, -69.75f, 8.06f};
    constexpr int kIceCreamVanGlobal = 9568;
    // Uzi Rider: streets of Hepburn Heights toured for Diablos, untested guesses around the mission's blip (940, -220).
    const std::vector<CVector> kHepburnHeights{{940.0f, -220.0f, 5.0f}, {900.0f, -260.0f, 5.0f}, {980.0f, -180.0f, 5.0f}};
    const CVector kQuaysPark{1213.13f, -1125.97f, 12.2f};
    const std::vector<int> kIceCreamGang{9572, 9576, 9580, 9584};
    // Trial By Fire: the flamethrower on Chinatown's edge and the streets toured for Triads; Big'N'Veiny: the van (g9744), the thief (g9712) and the box behind XXX Mags.
    const CVector kElBurrosFlamethrower{879.19f, -810.0f, 14.5f};
    const std::vector<CVector> kChinatownStreets{{936.0f, -846.0f, 14.5f}, {885.0f, -850.0f, 14.5f}, {990.0f, -830.0f, 14.5f}, {950.0f, -895.0f, 14.5f}};
    constexpr int kMagsVanGlobal = 9744;
    constexpr int kMagsThiefGlobal = 9712;
    const CVector kXXXMags{973.88f, -428.25f, 15.0f};
    const CVector kXXXMagsStop{973.63f, -428.06f, 15.5f};

    // King Courtney: the waste ground opposite the stadium; Uzi Rider: the Yardies' car (g14096) and the two Yardies (g14100, g14104).
    const CVector kWasteGround{50.0f, 70.0f, 16.0f};
    constexpr int kYardiesCarGlobal = 14096;
    const CVector kYardiesPickupCorner{108.0f, -277.0f, 10.0f};
    const CVector kYardiesPickupOppositeCorner{118.0f, -267.0f, 25.0f};
    // Gangcar Round-Up: mission garage g292 (257..263, -803..-795.5) and the count of cars delivered (g14224); a car teleported inside fell through its floor (to -11 m), so it is driven in through the door on its north side (where the mission plays its sound, 260, -790), facing south, once the door is open (a car hitting a wall is damaged, and the garage only takes one in good shape), and the player gets out north of it.
    constexpr int kRoundUpGarageGlobal = 292;
    constexpr int kRoundUpDeliveredGlobal = 14224;
    const CVector kRoundUpApproach{260.0f, -788.0f, 27.5f};
    const CVector kRoundUpOutside{260.0f, -786.0f, 27.5f};

    // Uzi Money: streets of Wichita Gardens around D-Ice's phone, 30 m apart, untested.
    const std::vector<CVector> kWichitaGardens{{-442.0f, -7.0f, 4.0f}, {-412.0f, -7.0f, 4.0f}, {-442.0f, 23.0f, 4.0f}, {-472.0f, -7.0f, 4.0f}, {-442.0f, -37.0f, 4.0f}};

    // Ends a job as its player would, with the side mission button again in its vehicle (getting out only starts a minute's countdown to get back in).
    void CancelJob() { Actions::PressSubMissionButton(); }

    // The jobs' start: the street by Marty's factory gates in Portland, where cars land well (in front of Luigi's club, a teleported taxi fell into the subway below).
    const CVector kJobStart{1190.0f, -795.0f, 14.5f};
    // Portland streets cars have landed well on in the missions, toured to meet fares and patients.
    const std::vector<CVector> kPortlandStreets{{1190.0f, -795.0f, 14.5f}, {936.0f, -846.0f, 14.5f}, {937.9f, -259.8f, 14.0f}, {1191.7f, -870.0f, 15.0f}};
    // The starts of the jobs counted per island: a street by the Staunton police station and a Shoreside Vale road, where cars landed well in the missions.
    const CVector kStauntonJobStart{327.7f, -1066.0f, 25.0f};
    const CVector kShoresideJobStart{-864.44f, -282.44f, 32.5f};

    // RC Toyz: the Toyz van and the four markers (LOCATE_PLAYER_ANY_MEANS_3D 5 m), Diablo Destruction and Mafia Massacre in Portland, Rumpo Rampage in Shoreside Vale, Casino Calamity in Staunton.
    constexpr int kToyzVan = 149;
    const CVector kDiabloDestructionStart{1014.0f, -120.0f, 5.0f};
    const CVector kMafiaMassacreStart{1158.0f, -309.0f, 23.0f};
    const CVector kRumpoRampageStart{-636.0f, 65.0f, 19.0f};
    const CVector kCasinoCalamityStart{366.0f, -1312.0f, 26.0f};

    // The Patriot challenges and Multistorey Mayhem: the middles of the areas their vehicles must be got into (untested heights: the ground is searched).
    const CVector kPatriotPlaygroundStart{1305.0f, -647.0f, 15.0f};
    const CVector kRideInTheParkStart{63.0f, -590.0f, 26.0f};
    const CVector kGrippedStart{-220.0f, 265.0f, 30.0f};
    const CVector kMultistoreyStart{252.0f, -540.0f, 26.0f};

    // D-Ice: the toy van (g16664), D-Ice's rigged car (g16752) and his St Mark's garage (g356, 1346.88..1352.5, -315.5..-310.69), the bullion carried (g16828) and the drop-off garage (g360), his brother (g16976).
    constexpr int kToyVanGlobal = 16664;
    // Rigged To Blow: where D-Ice's car waits and must be brought back (LOCATE_STOPPED_CAR_3D 3 x 4 m).
    const CVector kDIcesCarSpot{-682.0f, 76.0f, 17.375f};
    constexpr int kRiggedCarGlobal = 16752;
    const CVector kStMarksGarageParking{1349.69f, -313.1f, 50.5f};
    constexpr int kBullionCarriedGlobal = 16828;
    const CVector kBullionGarage{-834.1f, -168.6f, 34.0f};
    // Bullion Run: the airport strip where the platinum lies, as pickups without blips (30 to collect).
    const CVector kBullionField{-1080.0f, -163.19f, 12.0f};
    constexpr int kDIcesBrotherGlobal = 16976;
    // Rumble: the nine Red Jacks given bats (g16860 to g16892).
    const std::vector<int> kRumbleGang{16860, 16864, 16868, 16872, 16876, 16880, 16884, 16888, 16892};
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
        // The jobs, started with the side mission button in their vehicle and played until their reward, then ended by leaving the vehicle; they need no story mission, so Portland's come first and each island's once it is open.
        // Vigilante: 10 then 20 criminals on each island (counted in g4300, g4304, g4308 for the island loaded when the job starts), which set g4320, g4324, g4328 to 1 then 2.
        {.script = "copcar", .passedFlag = 0, .done = GlobalAtLeast(4320, 2), .start = kJobStart, .startModel = MODEL_POLICE, .startWithButton = true, .steps = {
            {"eliminate 20 criminals in Portland", GlobalAtLeast(4320, 2), Both(KeepAlive, Both(LogGlobals({4300}), Both(KillMarkedCarOccupants, KillMarked))), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        // Firefighter: 20 fires on each island (counted in g4332, g4336, g4340 for the island loaded when the job starts), which sets g4220.
        {.script = "firetru", .passedFlag = 0, .done = GlobalAtLeast(4332, 20), .start = kJobStart, .startModel = 97, .startWithButton = true, .steps = {
            {"put out 20 fires in Portland", GlobalAtLeast(4332, 20), Both(KeepAlive, Both(LogGlobals({4332}), ExtinguishMarkedCars)), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        // Paramedic: up to level 12, rewarded by never getting tired (no passed flag).
        {.script = "ambulan", .passedFlag = 0, .done = NeverTired, .start = kJobStart, .startModel = 106, .startWithButton = true, .steps = {
            {"rescue the patients up to level 12", NeverTired, Both(KeepAlive, FerryMarked(kPortlandStreets)), 3600},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        // Taxi Driver: 100 fares (g1596, which also opens the Borgnine).
        {.script = "taxi", .passedFlag = 1596, .start = kJobStart, .startModel = MODEL_TAXI, .startWithButton = true, .steps = {
            // The taxi is kept repaired: a car under 500 health ends the job.
            {"drive 100 fares (g1580 counts them)", GlobalAtLeast(1596, 1), Both(KeepAlive, Both(LogGlobals({1580}), FerryMarked(kPortlandStreets, true))), 3600},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
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
        // The first visit always ends after 8-Ball asks for the money (g1100 becomes 1), money or not: the run replays it once, as a player comes back.
        {.script = "frank3", .passedFlag = 1080, .start = kBombShopMarker, .beforeStart = EnsureMoney(100000), .steps = {
            {"get a car for 8-Ball", PassengersAtLeast(1), GetCar, 120},
            {"drive 8-Ball to the boat", BlipNear(kVantage), StopAt(kBoatEntryStop), 60},
            {"climb to the vantage point", [] { return OnFoot() && NoBlip(); }, WalkTo(kVantage)},
            {"pick off the guards while 8-Ball plants the bomb", UntilMissionEnds, KillScriptChars(kCartelGuards), 240},
        }},
        // Last Requests: Salvatore's car carries an armed ignition bomb; the mission only wants the player out of the Red Light District, then at the docks.
        {.script = "frank4", .passedFlag = 1084, .start = kSalvatoresMarker, .steps = {
            // The docks only get their blip once the player has entered the Red Light District, where the trapped car waits (IS_PLAYER_IN_ZONE REDLIGH).
            {"walk into the Red Light District, leaving the trapped car alone", BlipNear(kDocksBlip), WalkTo(kLuigisClubFront), 60},
            {"go to the docks", BlipNear(kStauntonDock), WalkTo(kDocks), 120},
            {"board Maria's boat", InCar, BoardScriptCar(kMariasBoatGlobal)},
            {"sail to Staunton with Maria and Asuka on deck", UntilMissionEnds, SailTo(kStauntonDock, {kMariaGlobal, kAsukaGlobal}), 120},
        }},
        // The Vigilante and Firefighter jobs on Staunton Island, open since Last Requests took the player there.
        {.script = "copcar", .passedFlag = 0, .done = GlobalAtLeast(4324, 2), .start = kStauntonJobStart, .startModel = MODEL_POLICE, .startWithButton = true, .steps = {
            {"eliminate 20 criminals on Staunton Island", GlobalAtLeast(4324, 2), Both(KeepAlive, Both(LogGlobals({4304}), Both(KillMarkedCarOccupants, KillMarked))), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        {.script = "firetru", .passedFlag = 0, .done = GlobalAtLeast(4336, 20), .start = kStauntonJobStart, .startModel = 97, .startWithButton = true, .steps = {
            {"put out 20 fires on Staunton Island", GlobalAtLeast(4336, 20), Both(KeepAlive, Both(LogGlobals({4336}), ExtinguishMarkedCars)), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
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
            {"answer the first pay phone", AnyBlipNear(kPhone2), AnswerPhone(kPhone1), 60},
            {"answer the second pay phone", AnyBlipNear(kPhone3), AnswerPhone(kPhone2), 60},
            {"answer the third pay phone", AnyBlipNear(kPhone4), AnswerPhone(kPhone3), 60},
            {"answer the last pay phone", BlipNear(kRaysMarker), AnswerPhone(kPhone4), 60},
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
            {"pick up the Kanbu", PassengersAtLeast(1), Both(ClearWanted, PickUpNearestFriend), 60},
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
            // The gang and the money only show up once Portland's collision is loaded (IS_COLLISION_IN_MEMORY 1), so once the player is there.
            // The yard is in Leone ground: once Sayonara Salvatore is played, the Mafia shoots Claude on sight and kills him within 2 s, so he is invulnerable there.
            {"go to the gang's yard in Portland", Marked(BLIP_CHAR), Both(Invulnerable, GoTo(kGangYard)), 60},
            {"punish the gang and take the money", BlipNear(kCasino), Both(Invulnerable, Both(KillMarked, CollectPickup)), 60},
            {"take the money back to the casino", UntilMissionEnds, Both(Invulnerable, StopAt(kCasino)), 60},
        }},
        // Smack Down: a dealer only appears within 90 m of the player, and shoots him; each kill (g11376) brings two more, and a waiting one leaves the streets after a delay down to 10 s.
        // The mission ends once none is left and passes with 8 kills: kill 8, then wait in Portland, far from them all, until the others have left.
        {.script = "kenji5", .passedFlag = 1332, .start = kKenjisMarker, .steps = {
            {"run down 8 Yardie dealers", GlobalAtLeast(kDealersKilledGlobal, 8), Both(KeepAlive, HuntMarked), 120},
            {"wait in Portland until the other dealers are off the streets", UntilMissionEnds, Both(KeepAlive, WalkTo(kLuigisClubFront)), 240},
        }},
        // Ray and Donald Love: Evidence Dash opens Liberator, Marked Man waits for both Plaster Blaster and A Drop in the Ocean.
        {.script = "ray2", .passedFlag = 1292, .start = kRaysMarker, .steps = {
            {"go to Phil's place", Marked(BLIP_CAR), GoToBlip, 90},
            // The fight ends at 12 Cartel killed (g12236); the last two come on foot from spawn points north of Phil's, only once a car has arrived or been hit while whole, and while those points are off screen: face south.
            {"fight off the Cartel", GlobalAtLeast(kCartelKilledGlobal, 12), Both(FaceHeading(180.0f), Both(KillMarked, WreckMarkedCars)), 300},
            // Phil is marked once the fight is over: stand by him rather than follow the radar.
            {"check on Phil", UntilMissionEnds, WalkTo(kBesidePhil), 60},
        }},
        {.script = "ray3", .passedFlag = 1296, .start = kRaysMarker, .steps = {
            {"get a car", InCar, GetCar},
            // The van only drops its next photo once the last one is picked up (within 1.5 m, by car or on foot), and each drop brings 2 stars.
            {"ram the evidence van and pick up each photo it drops", GlobalAtLeast(kPhotosCollectedGlobal, 6), Both(ClearWanted, Either(Marked(BLIP_OBJECT), CollectPickup, Both(StayNearScriptCar(kEvidenceVanGlobal), SetCarHealth(kEvidenceVanGlobal, 600)))), 180},
            {"leave the photos in a car", InScriptCar(kPhotosCarGlobal), GetCar},
            {"get out of it", OnFoot, GetOutAside},
            {"torch it", UntilMissionEnds, ExplodeScriptCar(kPhotosCarGlobal), 60},
        }},
        {.script = "ray4", .passedFlag = 1300, .start = kRaysMarker, .steps = {
            {"steal the police boat", InModel(120), BoardScriptCar(kPoliceBoatGlobal)},
            {"sink Ray's partner", UntilMissionEnds, Both(ExplodeScriptCar(kPartnersBoatGlobal), KillScriptChars({kPartnerGlobal})), 120},
        }},
        // A wrecked ambulance is a decoy and the next one comes out: once it has spotted the player (g12980), a hit below 900 makes the witness bail out in a bodycast, marked as an object, that an explosion finishes.
        {.script = "ray5", .passedFlag = 1304, .start = kRaysMarker, .steps = {
            {"get spotted by the ambulance", GlobalAtLeast(kAmbulanceAlertedGlobal, 1), StayNearScriptCar(kAmbulanceGlobal), 120},
            {"hit the ambulance until the witness bails out", Marked(BLIP_OBJECT), Both(ClearWanted, Both(StayNearScriptCar(kAmbulanceGlobal), SetCarHealth(kAmbulanceGlobal, 800))), 60},
            {"smash the witness's bodycast", UntilMissionEnds, Both(ClearWanted, ExplodeMarkedObjects), 60},
        }},
        // The hostage and his guards are created once the player sits in a gang car: as in the game, steal it near the hideout, so they are not created 1200 m away (stolen at Love's, he died on arrival).
        {.script = "love1", .passedFlag = 1340, .start = kLovesMarker, .steps = {
            {"go to the hideout in Fort Staunton", PlayerNear(kOutsideHideout, 3.0f), WalkTo(kOutsideHideout)},
            {"steal a Colombian gang car", InModel(kCartelCar), GetCarOf(kCartelCar)},
            {"come within 3 m of the old gentleman", BlipNear(kLovesGate), GoBesideScriptChar(kHostageGlobal, 2.5f), 60},
            {"wait for him to get in", PassengersAtLeast(1), JustWait, 60},
            {"drive him through the gate of Love's building", UntilMissionEnds, StopAt(kLovesGate), 60},
        }},
        // Waka-Gashira Wipeout! closes Kenji's missions for good: the run plays them first.
        {.script = "love2", .passedFlag = 1344, .start = kLovesMarker, .steps = {
            {"steal a Colombian gang car", InModel(kCartelCar), GetCarOf(kCartelCar)},
            {"drive to the top of the multi-storey", PlayerNear(kMultiStorey, 3.0f), GoTo(kMultiStorey)},
            {"whack Kenji", GlobalAtLeast(kKenjiWhackedGlobal, 1), KillScriptChars({kKenjiGlobal}), 60},
            // Passed once the player is out of Newport (zone COM_EAS) and out of the gang car.
            {"get out of Newport and dump the car", UntilMissionEnds, WalkTo(kLovesGate), 60},
        }},
        {.script = "love3", .passedFlag = 1348, .start = kLovesMarker, .steps = {
            {"take the boat", InScriptCar(kPackagesBoatGlobal), BoardScriptCar(kPackagesBoatGlobal)},
            // The plane drops a package every 7 s over the bay; they have no blip, and a police boat races for them.
            {"follow the plane and pick up the packages", BlipNear(kLovesMarker), SailToFloatingPackages, 240},
            {"take them back to Donald Love", UntilMissionEnds, WalkTo(kLovesMarker), 60},
        }},
        {.script = "ray6", .passedFlag = 1308, .start = kRaysMarker, .steps = {
            {"get a car for Ray", PassengersAtLeast(1), GetCar, 60},
            {"drive Ray to his flight", BlipNear(kRaysLockup), StopAt(kAirportStop), 120},
            // Within 2 m of the lock-up's door, the mission opens it and passes 13 s later.
            {"go to Ray's lock-up for his stash", UntilMissionEnds, WalkTo(kRaysLockupDoor), 60},
        }},
        // Grand Theft Aero closes Two-Faced Tanner for good: the run plays it first. At the airport the radar is followed (guards, the plane, the gang's car).
        // The gang at the Panlantic site is only created once Staunton's collision is loaded (IS_COLLISION_IN_MEMORY 2), so around a player teleported onto the lift: cross to Staunton first.
        {.script = "love4", .passedFlag = 1396, .start = kLovesMarker, .steps = {
            {"follow the radar at the airport hangar", BlipNear(kPanlanticLift), FollowRadar, 180},
            {"cross back to Staunton", PlayerNear(kLovesGate, 3.0f), WalkTo(kLovesGate)},
            {"kill the gang at the construction site and take the lift", BlipNear(kLovesMarker), Both(KillScriptChars(kPanlanticGang), StandAt(kPanlanticLift)), 120},
            {"take the package back to Donald Love", UntilMissionEnds, WalkTo(kLovesMarker), 60},
        }},
        {.script = "love5", .passedFlag = 1400, .start = kLovesMarker, .steps = {
            {"get a car", InCar, GetCar},
            // The truck only sets off with the player in a car within 15 m and its start on screen: park south of it, facing it, until it leaves its spot.
            {"pull up facing the truck so it sets off", Not(CarStoppedIn(kTruckGlobal, kTruckStartCorner, kTruckStartOppositeCorner)), Both(ParkAt(kFacingTheTruck, 0.0f), LogPointOnScreen(kTruckStart)), 60},
            // One step, so one action keeps the route point reached; the player stays by the truck, except in phase 3, when they scout the tunnel's exit.
            {"escort the truck into the Shoreside garage", CarStoppedIn(kTruckGlobal, kShoresideGarageCorner, kShoresideGarageOppositeCorner),
                Both(DriveScriptCarAlong(kTruckGlobal, kTruckPhaseGlobal, kTruckRoute), Either(AllOf(GlobalAtLeast(kTruckPhaseGlobal, 3), Not(GlobalAtLeast(kTruckPhaseGlobal, 4))), GoTo(kTunnelExitScout), StayNearScriptCar(kTruckGlobal))), 300},
            {"leave the garage so it closes", UntilMissionEnds, GoTo(kOutsideShoresideGarage), 60},
        }},
        // The script keeps the wanted level at 6 for the 3 minutes, and sends 8 rammers and 3 gunmen after the van, which a repair every 2 s does not outlast: the van is made proof, the player healed.
        // The cops also arrest a player stopped in a car: the van jumps every 2 s between two spots 330 m apart, both more than the 160 m from the warehouse the mission checks at the end.
        {.script = "love6", .passedFlag = 1404, .start = kLovesMarker, .steps = {
            {"get in the decoy Securicar", InScriptCar(kDecoyGlobal), Either(Marked(BLIP_CAR), EnterScriptCar(kDecoyGlobal), GoTo(kDecoyApproach)), 60},
            {"lead the cops away and hold out for 3 minutes", UntilMissionEnds, Both(ProofScriptCar(kDecoyGlobal), Both(TourPlaces({kFarFromWarehouse, kAlsoFarFromWarehouse}), KeepAlive)), 240},
        }},
        // Love's Disappearance is a cutscene that registers its pass.
        {.script = "love7", .passedFlag = 1408, .start = kLovesMarker, .steps = {
            {"watch the cutscene", UntilMissionEnds, JustWait, 60},
        }},
        // The Vigilante and Firefighter jobs in Shoreside Vale, open by then: Asuka's last missions play there.
        {.script = "copcar", .passedFlag = 0, .done = GlobalAtLeast(4328, 2), .start = kShoresideJobStart, .startModel = MODEL_POLICE, .startWithButton = true, .steps = {
            {"eliminate 20 criminals in Shoreside Vale", GlobalAtLeast(4328, 2), Both(KeepAlive, Both(LogGlobals({4308}), Both(KillMarkedCarOccupants, KillMarked))), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        {.script = "firetru", .passedFlag = 4220, .start = kShoresideJobStart, .startModel = 97, .startWithButton = true, .steps = {
            {"put out 20 fires in Shoreside Vale", GlobalAtLeast(4220, 1), Both(KeepAlive, Both(LogGlobals({4340}), ExtinguishMarkedCars)), 1800},
            {"cancel the job with the side mission button", UntilMissionEnds, CancelJob, 60},
        }},
        // Asuka's last missions, then The Exchange, the end of the story.
        {.script = "asusb1", .passedFlag = 1416, .start = kAsukasLastMarker, .steps = {
            {"get a car", InCar, GetCar},
            {"lure the three death squads into the Yakuza trap", UntilMissionEnds, LureMarkedCarsTo(kPikeCreekTrap), 300},
        }},
        {.script = "asusb2", .passedFlag = 1420, .start = kAsukasLastMarker, .steps = {
            // A stall only shows up once the player is within 150 m of it (or in its district), and its guard opens fire once it is hit: at each stop, smash the stall that showed up, then go on.
            {"smash the nine espresso stalls", UntilMissionEnds, Both(KeepAlive, Both(SmashMarkedObjectsNear(150.0f), TourPlaces(kEspressoStalls))), 600},
        }},
        {.script = "asusb3", .passedFlag = 1424, .start = kAsukasLastMarker, .steps = {
            {"take the boat", InScriptCar(kSamBoatGlobal), BoardScriptCar(kSamBoatGlobal)},
            // The buoy is a solid object (-805, -1310): a boat dropped on it throws the player out, and the mission only wants it within 15 m.
            // The plane is the game's drug-run Cessna, which only sets off once the 3:30 countdown (g16284) falls under 1:31: it is moved on to 1:32, and the plane shot down near the buoy, so its cargo falls in the bay.
            {"wait by the runway marker buoy and shoot the plane down", GlobalAtLeast(kCargoFallenGlobal, 1), Both(SailTo(kBesideRunwayBuoy, {}), Both(ShortenTimer(kPlaneCountdownGlobal, 92000), ShootDownDrugPlane(kBesideRunwayBuoy, 150.0f))), 240},
            {"collect the eight cargo packages", GlobalAtLeast(kCargoCollectedGlobal, 8), SailToNearestBlip, 240},
            // On foot in the Panlantic lift, under its low roof (LOCATE_PLAYER_ON_FOOT_3D 366.94, -328, 18.5).
            {"stash the cargo in the lift", UntilMissionEnds, StandAt(kPanlanticLift), 60},
        }},
        // The Exchange: once the ransom is paid the player is left unarmed among Catalina's men, healed here as her men are killed (Maria and Catalina spared).
        // Her helicopter (g17192) only sets off once the player leaves the dam; each area reached then creates the next gang, the last one Maria and Catalina, who takes off.
        {.script = "cat1", .passedFlag = 1472, .start = kExchangeMarker, .steps = {
            {"get out of the dam to the small dam (zone WEE_DAM)", AllOf(Marked(BLIP_CAR), PlayerNear(kSmallDam, 5.0f)), Both(KeepAlive, Both(KillScriptChars(kCatalinasGang), GoTo(kSmallDam))), 60},
            {"follow the helicopter to the first gang's area", PlayerNear(kCatalinasFirstArea, 5.0f), Both(KeepAlive, Both(KillScriptChars(kCatalinasGang), GoTo(kCatalinasFirstArea))), 60},
            {"follow the helicopter to Catalina's hideout", PlayerNear(kCatalinasHideout, 5.0f), Both(KeepAlive, Both(KillScriptChars(kCatalinasGang), GoTo(kCatalinasHideout))), 60},
            {"shoot Catalina's helicopter down as it takes off", Marked(BLIP_CHAR), Both(KeepAlive, Both(KillScriptChars(kCatalinasGang), ShootDownScriptHeli(kCatalinasHeliGlobal))), 120},
            {"rescue Maria", UntilMissionEnds, Both(KeepAlive, Both(KillScriptChars(kCatalinasGang), GoBesideScriptChar(kMariaGlobal, 3.0f))), 120},
        }},
        // The phone contacts, all open by the end of the story. Marty Chonks only calls between 9:00 and 18:59.
        {.script = "meat1", .passedFlag = 1156, .start = kMartysPhone, .beforeStart = SetClock(12), .steps = {
            {"get in Marty's car", InScriptCar(5932), EnterScriptCar(5932)},
            {"pick up the bank manager", BlipNear(kMartysYard), PickUpNearestFriend, 60},
            {"drive him to Marty's", BlipNear(kCrusher), StopAt(kMartysYard), 90},
            {"park under the crusher crane", CarStoppedIn(5932, kCrusherAreaCorner, kCrusherAreaOppositeCorner), ParkAt(kCrusherPad, kCrusherPadHeading)},
            {"get out and let the crane crush it", UntilMissionEnds, GetOutTo(kBesideCrusher), 90},
        }},
        {.script = "meat2", .passedFlag = 1160, .start = kMartysPhone, .beforeStart = SetClock(12), .steps = {
            {"get in Marty's car", InScriptCar(5988), EnterScriptCar(5988)},
            // The yard (1205.69, -789.19) lies within 20 m of the factory's door: the steps wait for the radar to empty, then for the next place blip, instead.
            {"pick up the two thieves", BlipNear(kMartysYard), PickUpNearestFriend, 90},
            {"drive them to Marty's yard, where they get out", NoBlip, StopAt(kMartysYard), 120},
            {"respray the car at the Pay 'n' Spray", Marked(BLIP_COORD), StopAt(kPayNSpray), 60},
            {"bring the car back to the factory and get out", UntilMissionEnds, Either(PlayerNear(kMartysFactory, 3.0f), GetOutAside, StopAt(kMartysFactory)), 60},
        }},
        {.script = "meat3", .passedFlag = 1164, .start = kMartysPhone, .beforeStart = SetClock(12), .steps = {
            {"get in Marty's car", InScriptCar(6068), EnterScriptCar(6068)},
            // East of her (1064, -378) the car fell through to -16 m (a subway entrance, it seems): stop west of her, within the 8 m she gets in from.
            {"pick up Marty's wife", BlipNear(kMartysYard), StopAt(kWestOfMrsChonks), 60},
            {"drive her to Marty's", Not(BlipNear(kMartysYard)), StopAt(kMartysYard), 120},
            {"take the car to the docks", AllOf(InScriptCar(6068), PlayerNear(kDocks, 3.0f)), DriveScriptCar(6068, GoTo(kDocks)), 60},
            {"get out on the quay", OnFoot, GetOutTo(kDocks)},
            {"push the car into the sea", UntilMissionEnds, MoveScriptCarTo(6068, kHarbourSea), 60},
        }},
        {.script = "meat4", .passedFlag = 1168, .start = kMartysPhone, .beforeStart = SetClock(12), .steps = {
            {"get in Marty's car", InScriptCar(6096), EnterScriptCar(6096)},
            // East of him (897, -476) the car spun and blew up: stop south of him, where he faces, within the 8 m he gets in from.
            {"pick up Carlos", BlipNear(kMartysBack), StopAt(kSouthOfCarlos), 60},
            {"drive him to Marty's back yard", UntilMissionEnds, StopAt(kMartysBack), 120},
        }},
        // Turismo can be raced again: Big'N'Veiny's passed routine clears g1112, so the run reads g1200, set by the first win and never cleared.
        {.script = "diablo1", .passedFlag = 1200, .start = kElBurrosPhone, .steps = {
            {"get a fast car", InCar, GetCar},
            {"line up on the starting grid", Marked(BLIP_CAR), StopAt(kTurismoGrid), 60},
            {"race through the checkpoints", UntilMissionEnds, AlternateCoordBlips, 300},
        }},
        {.script = "diablo2", .passedFlag = 1116, .start = kElBurrosPhone, .steps = {
            // A pickup far away has no object yet, so no blip either: walk to the bomb's spot in Harwood; the van is created (and marked) once it is collected.
            {"pick up El Burro's bomb", Marked(BLIP_CAR), WalkTo(kElBurrosBomb), 60},
            {"hijack the ice cream van", InModel(113), EnterBlipCar, 60},
            // The jingle brings the four gang members out (created 20 m from the van), and the mission passes once all four are dead, however they die.
            // The van rolls once parked: it is parked once, and the gang killed as soon as it shows up, the player healed, rather than re-parked under fire.
            {"park at Atlantic Quays and play the jingle", AnyCharNear(kIceCreamGang, kIceCreamVanGlobal, 50.0f), Both(ParkAt(kQuaysPark, 0.0f), Honk), 120},
            {"kill the lured gang", UntilMissionEnds, Both(KeepAlive, KillScriptChars(kIceCreamGang)), 60},
        }},
        {.script = "diablo3", .passedFlag = 1120, .start = kElBurrosPhone, .steps = {
            // The flamethrower hidden on Chinatown's edge starts a kill frenzy: 25 Triads (models 12 and 13) in 151 s, passed when the frenzy is; Chinatown's own Triads count with the six it creates.
            {"pick up El Burro's flamethrower", Frenzy, WalkTo(kElBurrosFlamethrower), 60},
            {"torch 25 Triads around Chinatown", UntilMissionEnds, Both(KeepAlive, Both(KillFrenzyTargets, TourPlaces(kChinatownStreets))), 180},
        }},
        {.script = "diablo4", .passedFlag = 1124, .start = kElBurrosPhone, .steps = {
            {"take the van", InScriptCar(kMagsVanGlobal), EnterScriptCar(kMagsVanGlobal)},
            {"follow the trail and waste the thief", BlipNear(kXXXMags), KillScriptChars({kMagsThiefGlobal}), 120},
            {"deliver the magazines behind XXX Mags", UntilMissionEnds, DriveScriptCar(kMagsVanGlobal, StopAt(kXXXMagsStop)), 60},
        }},
        // Bling-Bling Scramble can be raced again: Kingdom Come's passed routine clears g1356 and sets g1368, read too.
        {.script = "yard1", .passedFlag = 1356, .laterPassedFlag = 1368, .start = kCourtneysPhone, .steps = {
            {"get a car", InCar, GetCar},
            {"wait on the waste ground opposite the stadium", Marked(BLIP_CAR), StopAt(kWasteGround), 60},
            // A scramble of 15 checkpoints, each won by the first car on it, passed with strictly more than each rival: their drivers are killed (no explosion beside the player's car).
            {"race to the checkpoints", UntilMissionEnds, Both(KillMarkedCarOccupants, AlternateCoordBlips), 300},
        }},
        {.script = "yard2", .passedFlag = 1360, .start = kCourtneysPhone, .steps = {
            {"wait for the Yardies' car", CarStoppedIn(kYardiesCarGlobal, kYardiesPickupCorner, kYardiesPickupOppositeCorner), JustWait, 60},
            {"take the wheel", InScriptCar(kYardiesCarGlobal), EnterScriptCar(kYardiesCarGlobal)},
            // In Hepburn Heights (zone TOWERS) the mission counts the player's kills of Diablo models 14 and 15 (10 needed), then wants the car back on Yardie turf.
            // The radar marks Hepburn Heights a moment after, clears it on arrival, then marks the Yardies' turf once the 10 Diablos are dead.
            {"wait for Hepburn Heights to be marked", Marked(BLIP_COORD), JustWait},
            {"drive to Hepburn Heights, whose blip goes once there", NoBlip, GoTo(kHepburnHeights[0]), 60},
            {"drive-by 10 Diablos in Hepburn Heights", Marked(BLIP_COORD), Both(KeepAlive, Both(KillModelsNear({14, 15}), TourPlaces(kHepburnHeights))), 180},
            {"take the Yardies back to their turf", UntilMissionEnds, FollowRadarSparing({14100, 14104}), 120},
        }},
        {.script = "yard3", .passedFlag = 1364, .start = kCourtneysPhone, .steps = {
            {"boost a Diablo Stallion", InModel(137), GetCarOf(137)},
            {"wait at the Newport garage's door until it opens", GarageOpen(kRoundUpGarageGlobal), ParkAt(kRoundUpApproach, 180.0f), 30},
            {"drive it into the garage", CarInGarage(kRoundUpGarageGlobal), RollInto(kRoundUpApproach, 180.0f, 6.0f)},
            {"get out of the garage", GlobalAtLeast(kRoundUpDeliveredGlobal, 1), GetOutTo(kRoundUpOutside)},
            {"boost a Mafia Sentinel", InModel(134), GetCarOf(134)},
            {"wait at the Newport garage's door until it opens", GarageOpen(kRoundUpGarageGlobal), ParkAt(kRoundUpApproach, 180.0f), 30},
            {"drive it into the garage", CarInGarage(kRoundUpGarageGlobal), RollInto(kRoundUpApproach, 180.0f, 6.0f)},
            {"get out of the garage", GlobalAtLeast(kRoundUpDeliveredGlobal, 2), GetOutTo(kRoundUpOutside)},
            {"boost a Yakuza Stinger", InModel(136), GetCarOf(136)},
            {"wait at the Newport garage's door until it opens", GarageOpen(kRoundUpGarageGlobal), ParkAt(kRoundUpApproach, 180.0f), 30},
            {"drive it into the garage", CarInGarage(kRoundUpGarageGlobal), RollInto(kRoundUpApproach, 180.0f, 6.0f)},
            {"get out of the garage", UntilMissionEnds, GetOutTo(kRoundUpOutside), 60},
        }},
        // Read only in part: Catalina's letter turns the stash run into a fight; the radar is followed.
        {.script = "yard4", .passedFlag = 1368, .start = kCourtneysPhone, .steps = {
            {"follow the radar", UntilMissionEnds, FollowRadar, 300},
        }},
        // Uzi Money is a kill frenzy: only kills the game credits to the player's drive-by count.
        {.script = "hood1", .passedFlag = 1436, .start = kDIcesPhone, .steps = {
            {"get a car", InCar, GetCar},
            // A drive-by frenzy, 20 Nines (model 23) in 150 s, in zone PROJECT around D-Ice's phone: the streets nearby are toured for more of them.
            {"wait for the frenzy", Frenzy, JustWait},
            {"drive-by 20 Nines around Wichita Gardens", UntilMissionEnds, Both(KeepAlive, Both(KillFrenzyTargets, TourPlaces(kWichitaGardens))), 160},
        }},
        {.script = "hood2", .passedFlag = 1440, .start = kDIcesPhone, .steps = {
            {"take the toy van", InScriptCar(kToyVanGlobal), EnterScriptCar(kToyVanGlobal)},
            {"wreck the Nines' armoured vans", UntilMissionEnds, DestroyMarkedCarsExcept({kToyVanGlobal}), 300},
        }},
        // The St Mark's garage keeps the car (type 14): parked whole and still inside, it closes with the player in it.
        {.script = "hood3", .passedFlag = 1444, .start = kDIcesPhone, .steps = {
            {"take D-Ice's rigged car", InScriptCar(kRiggedCarGlobal), EnterScriptCar(kRiggedCarGlobal)},
            // The garage defuses the bomb (IS_CAR_IN_MISSION_GARAGE), then the car must go back undamaged, stopped where D-Ice left it.
            {"park it in the St Mark's garage", BlipNear(kDIcesCarSpot), ParkAt(kStMarksGarageParking, 0.0f), 120},
            {"take it back to D-Ice, undamaged, and get out", UntilMissionEnds, Either(PlayerNear(kDIcesCarSpot, 2.0f), GetOutAside, StopAt(kDIcesCarSpot)), 60},
        }},
        {.script = "hood4", .passedFlag = 1448, .start = kDIcesPhone, .steps = {
            {"collect the bullion and drop it off at the garage", UntilMissionEnds, CollectByCarAndDeliver(kBullionCarriedGlobal, 6, kBullionGarage, kBullionField), 600},
        }},
        {.script = "hood5", .passedFlag = 1452, .start = kDIcesPhone, .steps = {
            // Bats only: a gang member dying from any other weapon fails the mission; the player stays by D-Ice's brother, who leads to the fight.
            {"meet D-Ice's brother and win the rumble with a bat", UntilMissionEnds, Both(KeepAlive, Both(KillScriptCharsWith(kRumbleGang, WEAPONTYPE_BASEBALLBAT), GoBesideScriptChar(kDIcesBrotherGlobal, 3.0f))), 300},
        }},
        // RC Toyz: in the Toyz van (149) on a marker, 2 minutes to blow up a gang's cars with the RC buggy; passed with a new record (any car beats the first).
        {.script = "rc1", .passedFlag = 1636, .start = kDiabloDestructionStart, .startModel = kToyzVan, .steps = {
            {"blow up Diablo cars for 2 minutes", UntilMissionEnds, Both(KeepToyCarAway, BlowUpCarsOfModel(137)), 180},
        }},
        {.script = "rc2", .passedFlag = 1640, .start = kMafiaMassacreStart, .startModel = kToyzVan, .steps = {
            {"blow up Mafia cars for 2 minutes", UntilMissionEnds, Both(KeepToyCarAway, BlowUpCarsOfModel(134)), 180},
        }},
        {.script = "rc3", .passedFlag = 1644, .start = kRumpoRampageStart, .startModel = kToyzVan, .island = 2, .steps = {
            {"blow up Hood cars for 2 minutes", UntilMissionEnds, Both(KeepToyCarAway, BlowUpCarsOfModel(139)), 180},
        }},
        {.script = "rc4", .passedFlag = 1648, .start = kCasinoCalamityStart, .startModel = kToyzVan, .island = 1, .steps = {
            {"blow up Yakuza cars for 2 minutes", UntilMissionEnds, Both(KeepToyCarAway, BlowUpCarsOfModel(136)), 180},
        }},
        // Off-road and car park challenges: got into a car of the model in the start area, every checkpoint is marked at once, each touched in the car (LOCATE_PLAYER_IN_CAR_3D, 2 to 2.5 m).
        {.script = "t4x4_1", .passedFlag = 1512, .start = kPatriotPlaygroundStart, .startModel = 96, .steps = {
            {"touch every checkpoint of Patriot Playground", UntilMissionEnds, TouchCoordBlips, 300},
        }},
        {.script = "t4x4_2", .passedFlag = 1516, .start = kRideInTheParkStart, .startModel = 90, .island = 1, .steps = {
            {"touch every checkpoint of A Ride in the Park", UntilMissionEnds, TouchCoordBlips, 300},
        }},
        {.script = "t4x4_3", .passedFlag = 1520, .start = kGrippedStart, .startModel = 96, .island = 1, .steps = {
            {"touch every checkpoint of Gripped!", UntilMissionEnds, TouchCoordBlips, 300},
        }},
        {.script = "mayhem", .passedFlag = 1524, .start = kMultistoreyStart, .startModel = 129, .island = 1, .steps = {
            {"touch every checkpoint of Multistorey Mayhem", UntilMissionEnds, TouchCoordBlips, 300},
        }},
    };
    return run;
}

bool Sheets::Done(const Sheet& sheet) {
    if (sheet.done) return sheet.done();
    return Actions::ScriptGlobal(sheet.passedFlag) == 1 || (sheet.laterPassedFlag && Actions::ScriptGlobal(sheet.laterPassedFlag) == 1);
}

const Sheets::Sheet* Sheets::Find(const std::string& script) {
    const Sheet* first = nullptr;
    for (const Sheet& sheet : Run()) {
        if (script != sheet.script) continue;
        if (!Done(sheet)) return &sheet;
        if (!first) first = &sheet;
    }
    return first;
}
