import 'package:drift_flutter/drift_flutter.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

import 'database.dart';
import 'store.dart';

/// Die Datenbank der App. Tests ersetzen sie durch eine im Arbeitsspeicher.
final databaseProvider = Provider<ParloDatabase>((ref) {
  final db = ParloDatabase(driftDatabase(name: 'parlo'));
  ref.onDispose(db.close);
  return db;
});

final storeProvider = Provider<ParloStore>(
  (ref) => ParloStore(ref.watch(databaseProvider)),
);
