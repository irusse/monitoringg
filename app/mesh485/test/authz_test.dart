import 'package:flutter_test/flutter_test.dart';

import 'package:mesh485/client/fixture_client.dart';

void main() {
  test('unknown operator mutate/OPEN is BAD_AUTH', () async {
    final c = FixtureMesh485Client();
    expect((await c.accessOpen(operatorName: 'eve')).error, 'BAD_AUTH');
    expect((await c.accessLock(operatorName: 'eve')).error, 'BAD_AUTH');
    expect(
      (await c.accessAddCard(uid: '04A79122', operatorName: 'eve')).error,
      'BAD_AUTH',
    );
    expect(
      (await c.commandExecute(
        deviceId: 'PP-001',
        capability: 'CONTROL_RELAY',
        pointId: 'R1',
        action: 'ON',
        operatorName: 'eve',
      ))
          .error,
      'BAD_AUTH',
    );
    expect((await c.accessGetUsers(operatorName: 'eve')).error, 'BAD_AUTH');
  });

  test('factory viewer OPEN is NO_PERMISSION not BAD_AUTH', () async {
    final c = FixtureMesh485Client();
    final ack = await c.accessOpen(operatorName: 'viewer');
    expect(ack.error, 'NO_PERMISSION');
    expect(ack.denied, isTrue);
    expect(ack.badAuth, isFalse);
  });

  test('factory operator OPEN is honest TIMEOUT', () async {
    final c = FixtureMesh485Client();
    final ack = await c.accessOpen(operatorName: 'operator');
    expect(ack.error, 'TIMEOUT');
    expect(ack.badAuth, isFalse);
  });
}
