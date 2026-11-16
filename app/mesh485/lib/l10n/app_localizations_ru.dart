// ignore: unused_import
import 'package:intl/intl.dart' as intl;
import 'app_localizations.dart';

// ignore_for_file: type=lint

/// The translations for Russian (`ru`).
class AppLocalizationsRu extends AppLocalizations {
  AppLocalizationsRu([String locale = 'ru']) : super(locale);

  @override
  String get appTitle => 'MESH 485';

  @override
  String get appSubtitle => 'Консоль расширителя MESH 485';

  @override
  String get loginContinue => 'Продолжить';

  @override
  String get loginHint =>
      'Выберите роль. Живой USB/BLE — в Настройках. Управление только у оператора и администратора.';

  @override
  String get roleViewer => 'наблюдатель';

  @override
  String get roleOperator => 'оператор';

  @override
  String get roleAdmin => 'администратор';

  @override
  String get navDashboard => 'Дашборд';

  @override
  String get navObjects => 'Объекты';

  @override
  String get navDevices => 'Устройства';

  @override
  String get navEvents => 'События';

  @override
  String get navMesh => 'Меш';

  @override
  String get navBridge => 'Мост';

  @override
  String get navAccess => 'Доступ';

  @override
  String get navControl => 'Управление';

  @override
  String get navSettings => 'Настройки';

  @override
  String get statObjects => 'Объекты';

  @override
  String get statDevices => 'Устройства';

  @override
  String get statNormal => 'норма';

  @override
  String get statFault => 'неисправность';

  @override
  String get statAlarm => 'тревога';

  @override
  String get statExtender => 'Расширитель';

  @override
  String get online => 'в сети';

  @override
  String get offline => 'не в сети';

  @override
  String get ackMesh => 'сеть';

  @override
  String get ackDevice => 'прибор';

  @override
  String get ackTimeout => 'таймаут';

  @override
  String get dashboardHint =>
      'Path A: PP-001 без прибора на шине — FAULT / не в сети. Управление — только объявленные возможности, с подтверждением.';

  @override
  String get noObjects => 'Нет объектов';

  @override
  String get objectsNote =>
      'С2000-ПП = PP-001. Сигнал-10 = S10-001. Сигнал-20 = S20-001. Турникет = TS-001. Сырая шина = RAW-001. ПЦН = PCN-001.';

  @override
  String get siteBench => 'Стенд';

  @override
  String get capabilities => 'возможности';

  @override
  String get address => 'адрес';

  @override
  String get eventsEmpty => 'события: []';

  @override
  String get meshTitle => 'Состояние расширителя (не радиочат)';

  @override
  String get meshNode => 'Узел';

  @override
  String get meshRadio => 'Радио';

  @override
  String get meshRepeater => 'Повторитель';

  @override
  String get meshRssi => 'RSSI';

  @override
  String get meshSnr => 'SNR';

  @override
  String get meshRx => 'RX';

  @override
  String get meshTx => 'TX';

  @override
  String get meshUptime => 'Аптайм';

  @override
  String get meshLastSeen => 'Последний раз';

  @override
  String get meshProfile => 'Профиль';

  @override
  String get meshAdapters => 'Адаптеры';

  @override
  String get meshRs485 => 'RS-485';

  @override
  String get on => 'вкл';

  @override
  String get off => 'выкл';

  @override
  String get bridgeSession => 'Сессия';

  @override
  String get bridgeLevel => 'Уровень';

  @override
  String get bridgePort => 'Порт';

  @override
  String get bridgeOpenLater => 'BRIDGE_OPEN (не операторское действие)';

  @override
  String get bridgeHint =>
      'По умолчанию READ_ONLY. Оператор не открывает сессию моста. COMMAND_EXECUTE — экран Управление.';

  @override
  String get accessMissing => 'TS-001 нет в реестре';

  @override
  String get accessOpen => 'OPEN';

  @override
  String get accessLock => 'LOCK';

  @override
  String accessHint(String caps) {
    return 'OPEN = ACCESS_OPEN, LOCK = ACCESS_LOCK (не COMMAND_EXECUTE). Фото только с диска, не в LoRa. Возможности: $caps.';
  }

  @override
  String get accessViewerDenied =>
      'Наблюдатель не может OPEN/LOCK и не меняет карты. Войдите как оператор или администратор.';

  @override
  String get accessOperatorHint =>
      'OPEN и LOCK — с подтверждением. Карты ACL меняет только администратор.';

  @override
  String get accessAdminListHint =>
      'Полный список карт ACL — только администратор (ACCESS_GET_USERS). Наблюдатель видит число в STATUS.';

  @override
  String get accessConfirmTitle => 'Подтвердить OPEN';

  @override
  String get accessConfirmBody =>
      'TS-001 · ACCESS_OPEN. Будет виден ACK сети, ACK прибора или таймаут.';

  @override
  String get accessLockConfirmTitle => 'Подтвердить LOCK';

  @override
  String get accessLockConfirmBody =>
      'TS-001 · ACCESS_LOCK. Будет виден ACK сети, ACK прибора или таймаут.';

  @override
  String get accessEmployees => 'Сотрудники / карты';

  @override
  String get accessAddEmployee => 'Добавить';

  @override
  String get accessEditEmployee => 'Карта';

  @override
  String get accessName => 'Имя';

  @override
  String get accessEmployeeId => 'employee_id';

  @override
  String get accessUid => 'UID (hex)';

  @override
  String get accessUidRequired => 'Нужен UID карты';

  @override
  String get accessEnabled => 'Разрешена';

  @override
  String get accessDisabled => 'отключена';

  @override
  String get accessPhotoPath => 'Фото (путь на диске)';

  @override
  String get accessPhotoHint =>
      'Только локальный файл. Байты фото не уходят на узел / LoRa / OLED.';

  @override
  String get accessSave => 'Сохранить';

  @override
  String get accessRemove => 'Удалить';

  @override
  String get accessEmptyEmployees => 'На узле нет карт (пустой ACL = DENY).';

  @override
  String get accessEvents => 'События турникета TS-001';

  @override
  String get accessBusy => 'Узел BUSY (ACL полон, cap 64). Карта не добавлена.';

  @override
  String accessStatusLine(String state, int users) {
    return 'Состояние $state · users $users';
  }

  @override
  String get settingsRole => 'Роль';

  @override
  String get settingsTransport => 'Транспорт';

  @override
  String get settingsServer => 'URL сервера';

  @override
  String get settingsServerNone => '(нет — LAN REST позже)';

  @override
  String get settingsNode => 'Узел';

  @override
  String get settingsPort => 'Протокол порта / скорость';

  @override
  String settingsPortHint(String protocol, int baud) {
    return '$protocol $baud 8N1 (только просмотр — GPIO в прошивке)';
  }

  @override
  String get settingsPortPicker => 'Порт / устройство';

  @override
  String get settingsFixture => 'fixture (JSON в памяти)';

  @override
  String get settingsRefresh => 'Обновить';

  @override
  String get settingsBlePin =>
      'BLE PIN MeshCore: 123456 (системный диалог Android).';

  @override
  String get settingsBleGap =>
      'BLE NUS: байт открывающей скобки JSON идёт в MESH 485, не в Companion. Не оборачивать JSON в кадры 0x3c/0x3e. USB-OTG — запасной путь, если пары нет.';

  @override
  String get filterDevice => 'Прибор';

  @override
  String get filterType => 'Тип';

  @override
  String get filterAll => 'все';

  @override
  String get connectFailed => 'Нет порта / нет ответа — показан fixture.';

  @override
  String seconds(int n) {
    return '$n с';
  }

  @override
  String get controlEmpty =>
      'Нет разрешённых команд. Реле только если адаптер объявил CONTROL_RELAY и конфиг панели разрешил именованную точку (R1…).';

  @override
  String get controlHint =>
      'Нет команды «отключить пожарную тревогу» / «снять все». OPEN турникета — экран Доступ (ACCESS_OPEN). Нужно подтверждение.';

  @override
  String get controlViewerDenied =>
      'Наблюдатель не может выполнять COMMAND_EXECUTE. Войдите как оператор или администратор.';

  @override
  String get controlConfirmTitle => 'Подтвердить управление';

  @override
  String controlConfirmBody(
    String device,
    String cap,
    String point,
    String action,
  ) {
    return '$device · $cap · $point · $action. Будет виден ACK сети, ACK прибора или таймаут.';
  }

  @override
  String get controlConfirm => 'Выполнить';

  @override
  String get controlCancel => 'Отмена';

  @override
  String get controlLastResult => 'Последний результат';

  @override
  String get controlOn => 'ВКЛ';

  @override
  String get controlOff => 'ВЫКЛ';
}
