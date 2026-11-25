/// Der Schlaegerkatalog der Firmware (lib/core/src/core/clubs.h), in
/// derselben Reihenfolge. Der Kurzname steht in gespeicherten Runden und
/// aendert sich nie.
enum Club {
  driver('Dr', 'Driver', 210),
  wood3('H3', 'Holz 3', 190),
  wood5('H5', 'Holz 5', 180),
  wood7('H7', 'Holz 7', 170),
  hybrid('Hy', 'Hybrid', 175),
  iron3('E3', 'Eisen 3', 180),
  iron4('E4', 'Eisen 4', 170),
  iron5('E5', 'Eisen 5', 160),
  iron6('E6', 'Eisen 6', 150),
  iron7('E7', 'Eisen 7', 140),
  iron8('E8', 'Eisen 8', 128),
  iron9('E9', 'Eisen 9', 116),
  pitchingWedge('PW', 'Pitching Wedge', 100),
  gapWedge('GW', 'Gap Wedge', 85),
  sandWedge('SW', 'Sand Wedge', 70),
  lobWedge('LW', 'Lob Wedge', 55),
  putter('Pt', 'Putter', 0);

  const Club(this.shortName, this.label, this.defaultCarryM);

  final String shortName;
  final String label;

  /// Schlagweite ab Werk in Metern; der Putter hat keine.
  final int defaultCarryM;

  static final Map<String, Club> _byShortName = {
    for (final c in values) c.shortName: c,
  };

  static Club? fromShortName(String shortName) => _byShortName[shortName];
}

const int maxBagClubs = 14;
const int minCarryM = 1;
const int maxCarryM = 400;

/// Bag ab Werk: Driver, Holz 3, Hybrid, Eisen 5 bis 9, Pitching Wedge,
/// Sand Wedge, Putter.
const Set<Club> defaultBag = {
  Club.driver,
  Club.wood3,
  Club.hybrid,
  Club.iron5,
  Club.iron6,
  Club.iron7,
  Club.iron8,
  Club.iron9,
  Club.pitchingWedge,
  Club.sandWedge,
  Club.putter,
};
