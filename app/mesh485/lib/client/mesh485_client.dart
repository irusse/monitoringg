import '../models/access_user.dart';
import '../models/command_ack.dart';
import '../models/device.dart';
import '../models/mesh_event.dart';
import '../models/node_health.dart';
import '../models/point.dart';

/// MESH 485 companion seam.
///
/// Live USB CDC (Windows) / BLE NUS (Android) via [LiveMesh485Client].
/// [FixtureMesh485Client] is the fallback when no port is present.
abstract class Mesh485Client {
  /// Human label for Settings (e.g. `fixture (in-memory JSON)`).
  String get transportLabel;

  Future<List<DeviceRecord>> discoverDevices();

  Future<DeviceRecord> deviceInfo(String deviceId);

  Future<List<PointRecord>> pointList({String? deviceId});

  Future<List<MeshEvent>> eventList();

  Future<NodeHealth> nodeStatus();

  Future<BridgeStatus> bridgeStatus();

  Future<List<SiteObject>> listObjects();

  /// OPERATOR+ ACCESS_OPEN (not COMMAND_EXECUTE). Fixture is honest TIMEOUT.
  Future<CommandAck> accessOpen({String operatorName = 'viewer'});

  /// OPERATOR+ ACCESS_LOCK. Fixture is honest TIMEOUT (no GPIO).
  Future<CommandAck> accessLock({String operatorName = 'viewer'});

  /// VIEWER ACCESS_STATUS (users = enabled ACL count).
  Future<AccessStatus> accessStatus({String operatorName = 'viewer'});

  /// ADMIN ACCESS_GET_USERS dump. VIEWER/OPERATOR → NO_PERMISSION. No photo bytes.
  Future<AccessUsersPage> accessGetUsers({
    String operatorName = 'viewer',
    int offset = 0,
    int limit = 16,
  });

  /// ADMIN ACCESS_ADD_CARD upsert. Overflow → BUSY.
  Future<CommandAck> accessAddCard({
    required String uid,
    String? employeeId,
    String? name,
    bool enabled = true,
    String operatorName = 'viewer',
  });

  /// ADMIN ACCESS_REMOVE_CARD.
  Future<CommandAck> accessRemoveCard({
    required String uid,
    String operatorName = 'viewer',
  });

  /// VIEWER ACCESS_GET_EVENTS (TS-001). Same shape as EVENT_LIST rows.
  Future<List<MeshEvent>> accessGetEvents({String operatorName = 'viewer'});

  /// OPERATOR+ only. Firmware refuses VIEWER, forbidden caps, and unmapped relays.
  Future<CommandAck> commandExecute({
    required String deviceId,
    required String capability,
    required String pointId,
    required String action,
    String operatorName = 'viewer',
  });
}
