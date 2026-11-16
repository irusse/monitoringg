import 'dart:io';

import 'package:flutter/material.dart';

/// Disk photo next to the employee in Flutter only. Path is never MESH 485 JSON.
Widget accessPhotoAvatar(String? path, {double radius = 22}) {
  if (path == null || path.isEmpty) {
    return CircleAvatar(
      radius: radius,
      child: Icon(Icons.person_outline, size: radius),
    );
  }
  final file = File(path);
  if (!file.existsSync()) {
    return CircleAvatar(
      radius: radius,
      child: Icon(Icons.image_outlined, size: radius),
    );
  }
  return CircleAvatar(
    radius: radius,
    backgroundImage: FileImage(file),
  );
}
