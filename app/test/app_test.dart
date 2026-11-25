import 'dart:io';

import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:material_ui/material_ui.dart';
import 'package:parlo/app.dart';
import 'package:parlo/app_info.dart';
import 'package:parlo/router.dart';

Widget app({String start = Routes.rounds}) =>
    ProviderScope(child: ParloApp(initialLocation: start));

void main() {
  testWidgets('startet im Bereich Runden', (tester) async {
    await tester.pumpWidget(app());
    await tester.pumpAndSettle();

    expect(find.text('Noch keine Runden'), findsOneWidget);
    expect(find.byType(NavigationBar), findsOneWidget);
    expect(
      tester.widget<NavigationBar>(find.byType(NavigationBar)).selectedIndex,
      0,
    );
  });

  testWidgets('die Leiste wechselt zwischen allen vier Bereichen', (
    tester,
  ) async {
    await tester.pumpWidget(app());
    await tester.pumpAndSettle();

    const erwartet = {
      'Plätze': 'Noch keine Plätze',
      'Gerät': 'Kein Gerät verbunden',
      'Einstellungen': 'Version $appVersion',
      'Runden': 'Noch keine Runden',
    };
    var index = 1;
    for (final eintrag in erwartet.entries) {
      await tester.tap(
        find.descendant(
          of: find.byType(NavigationBar),
          matching: find.text(eintrag.key),
        ),
      );
      await tester.pumpAndSettle();
      expect(find.text(eintrag.value), findsOneWidget, reason: eintrag.key);
      expect(
        tester.widget<NavigationBar>(find.byType(NavigationBar)).selectedIndex,
        index % 4,
        reason: eintrag.key,
      );
      index++;
    }
  });

  testWidgets('jeder Bereich ist direkt erreichbar', (tester) async {
    const ziele = {
      Routes.rounds: 'Noch keine Runden',
      Routes.courses: 'Noch keine Plätze',
      Routes.device: 'Kein Gerät verbunden',
      Routes.settings:
          'Schlägerauswahl, Schlagweiten und Anzeige des Geräts '
          'folgen hier.',
    };
    for (final ziel in ziele.entries) {
      await tester.pumpWidget(app(start: ziel.key));
      await tester.pumpAndSettle();
      expect(find.text(ziel.value), findsOneWidget, reason: ziel.key);
      await tester.pumpWidget(const SizedBox());
    }
  });

  testWidgets('kein Ueberlauf auf kleinem Bildschirm mit grosser Schrift', (
    tester,
  ) async {
    tester.view.physicalSize = const Size(320, 480);
    tester.view.devicePixelRatio = 1;
    tester.platformDispatcher.textScaleFactorTestValue = 2;
    addTearDown(tester.view.reset);
    addTearDown(tester.platformDispatcher.clearTextScaleFactorTestValue);

    for (final ziel in [
      Routes.rounds,
      Routes.courses,
      Routes.device,
      Routes.settings,
    ]) {
      await tester.pumpWidget(app(start: ziel));
      await tester.pumpAndSettle();
      expect(tester.takeException(), isNull, reason: ziel);
      await tester.pumpWidget(const SizedBox());
    }
  });

  test('Versionen stimmen mit pubspec und Firmware ueberein', () {
    final pubspec = File('pubspec.yaml').readAsStringSync();
    expect(pubspec, contains('version: $appVersion+'));

    final api = File('../lib/core/src/core/api.h').readAsStringSync();
    final treffer = RegExp(r'kApiVersion\s*=\s*(\d+)').firstMatch(api);
    expect(treffer, isNotNull, reason: 'kApiVersion nicht gefunden');
    expect(int.parse(treffer!.group(1)!), supportedApiVersion);
  });
}
