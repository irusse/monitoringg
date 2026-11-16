import 'package:flutter/material.dart';

/// Web / tests without dart:io: path is local-only metadata, never on the wire.
Widget accessPhotoAvatar(String? path, {double radius = 22}) {
  return CircleAvatar(
    radius: radius,
    child: Icon(
      path != null && path.isNotEmpty ? Icons.image_outlined : Icons.person_outline,
      size: radius,
    ),
  );
}
