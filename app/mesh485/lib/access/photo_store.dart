/// Local photo paths keyed by employee_id (or uid). Never sent on MESH 485.
class PhotoStore {
  final Map<String, String> _paths = {};

  String? pathFor(String key) {
    final p = _paths[key];
    if (p == null || p.isEmpty) return null;
    return p;
  }

  void setPath(String key, String path) {
    if (key.isEmpty) return;
    if (path.isEmpty) {
      _paths.remove(key);
    } else {
      _paths[key] = path;
    }
  }
}
