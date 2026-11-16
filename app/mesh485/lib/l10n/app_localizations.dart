import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_localizations/flutter_localizations.dart';
import 'package:intl/intl.dart' as intl;

import 'app_localizations_ru.dart';

// ignore_for_file: type=lint

/// Callers can lookup localized strings with an instance of AppLocalizations
/// returned by `AppLocalizations.of(context)`.
///
/// Applications need to include `AppLocalizations.delegate()` in their app's
/// `localizationDelegates` list, and the locales they support in the app's
/// `supportedLocales` list. For example:
///
/// ```dart
/// import 'l10n/app_localizations.dart';
///
/// return MaterialApp(
///   localizationsDelegates: AppLocalizations.localizationsDelegates,
///   supportedLocales: AppLocalizations.supportedLocales,
///   home: MyApplicationHome(),
/// );
/// ```
///
/// ## Update pubspec.yaml
///
/// Please make sure to update your pubspec.yaml to include the following
/// packages:
///
/// ```yaml
/// dependencies:
///   # Internationalization support.
///   flutter_localizations:
///     sdk: flutter
///   intl: any # Use the pinned version from flutter_localizations
///
///   # Rest of dependencies
/// ```
///
/// ## iOS Applications
///
/// iOS applications define key application metadata, including supported
/// locales, in an Info.plist file that is built into the application bundle.
/// To configure the locales supported by your app, you’ll need to edit this
/// file.
///
/// First, open your project’s ios/Runner.xcworkspace Xcode workspace file.
/// Then, in the Project Navigator, open the Info.plist file under the Runner
/// project’s Runner folder.
///
/// Next, select the Information Property List item, select Add Item from the
/// Editor menu, then select Localizations from the pop-up menu.
///
/// Select and expand the newly-created Localizations item then, for each
/// locale your application supports, add a new item and select the locale
/// you wish to add from the pop-up menu in the Value field. This list should
/// be consistent with the languages listed in the AppLocalizations.supportedLocales
/// property.
abstract class AppLocalizations {
  AppLocalizations(String locale)
    : localeName = intl.Intl.canonicalizedLocale(locale.toString());

  final String localeName;

  static AppLocalizations of(BuildContext context) {
    return Localizations.of<AppLocalizations>(context, AppLocalizations)!;
  }

  static const LocalizationsDelegate<AppLocalizations> delegate =
      _AppLocalizationsDelegate();

  /// A list of this localizations delegate along with the default localizations
  /// delegates.
  ///
  /// Returns a list of localizations delegates containing this delegate along with
  /// GlobalMaterialLocalizations.delegate, GlobalCupertinoLocalizations.delegate,
  /// and GlobalWidgetsLocalizations.delegate.
  ///
  /// Additional delegates can be added by appending to this list in
  /// MaterialApp. This list does not have to be used at all if a custom list
  /// of delegates is preferred or required.
  static const List<LocalizationsDelegate<dynamic>> localizationsDelegates =
      <LocalizationsDelegate<dynamic>>[
        delegate,
        GlobalMaterialLocalizations.delegate,
        GlobalCupertinoLocalizations.delegate,
        GlobalWidgetsLocalizations.delegate,
      ];

  /// A list of this localizations delegate's supported locales.
  static const List<Locale> supportedLocales = <Locale>[Locale('ru')];

  /// No description provided for @appTitle.
  ///
  /// In ru, this message translates to:
  /// **'MESH 485'**
  String get appTitle;

  /// No description provided for @appSubtitle.
  ///
  /// In ru, this message translates to:
  /// **'Консоль расширителя MESH 485'**
  String get appSubtitle;

  /// No description provided for @loginContinue.
  ///
  /// In ru, this message translates to:
  /// **'Продолжить'**
  String get loginContinue;

  /// No description provided for @loginHint.
  ///
  /// In ru, this message translates to:
  /// **'Выберите роль. Живой USB/BLE — в Настройках. Управление только у оператора и администратора.'**
  String get loginHint;

  /// No description provided for @roleViewer.
  ///
  /// In ru, this message translates to:
  /// **'наблюдатель'**
  String get roleViewer;

  /// No description provided for @roleOperator.
  ///
  /// In ru, this message translates to:
  /// **'оператор'**
  String get roleOperator;

  /// No description provided for @roleAdmin.
  ///
  /// In ru, this message translates to:
  /// **'администратор'**
  String get roleAdmin;

  /// No description provided for @navDashboard.
  ///
  /// In ru, this message translates to:
  /// **'Дашборд'**
  String get navDashboard;

  /// No description provided for @navObjects.
  ///
  /// In ru, this message translates to:
  /// **'Объекты'**
  String get navObjects;

  /// No description provided for @navDevices.
  ///
  /// In ru, this message translates to:
  /// **'Устройства'**
  String get navDevices;

  /// No description provided for @navEvents.
  ///
  /// In ru, this message translates to:
  /// **'События'**
  String get navEvents;

  /// No description provided for @navMesh.
  ///
  /// In ru, this message translates to:
  /// **'Меш'**
  String get navMesh;

  /// No description provided for @navBridge.
  ///
  /// In ru, this message translates to:
  /// **'Мост'**
  String get navBridge;

  /// No description provided for @navAccess.
  ///
  /// In ru, this message translates to:
  /// **'Доступ'**
  String get navAccess;

  /// No description provided for @navControl.
  ///
  /// In ru, this message translates to:
  /// **'Управление'**
  String get navControl;

  /// No description provided for @navSettings.
  ///
  /// In ru, this message translates to:
  /// **'Настройки'**
  String get navSettings;

  /// No description provided for @statObjects.
  ///
  /// In ru, this message translates to:
  /// **'Объекты'**
  String get statObjects;

  /// No description provided for @statDevices.
  ///
  /// In ru, this message translates to:
  /// **'Устройства'**
  String get statDevices;

  /// No description provided for @statNormal.
  ///
  /// In ru, this message translates to:
  /// **'норма'**
  String get statNormal;

  /// No description provided for @statFault.
  ///
  /// In ru, this message translates to:
  /// **'неисправность'**
  String get statFault;

  /// No description provided for @statAlarm.
  ///
  /// In ru, this message translates to:
  /// **'тревога'**
  String get statAlarm;

  /// No description provided for @statExtender.
  ///
  /// In ru, this message translates to:
  /// **'Расширитель'**
  String get statExtender;

  /// No description provided for @online.
  ///
  /// In ru, this message translates to:
  /// **'в сети'**
  String get online;

  /// No description provided for @offline.
  ///
  /// In ru, this message translates to:
  /// **'не в сети'**
  String get offline;

  /// No description provided for @ackMesh.
  ///
  /// In ru, this message translates to:
  /// **'сеть'**
  String get ackMesh;

  /// No description provided for @ackDevice.
  ///
  /// In ru, this message translates to:
  /// **'прибор'**
  String get ackDevice;

  /// No description provided for @ackTimeout.
  ///
  /// In ru, this message translates to:
  /// **'таймаут'**
  String get ackTimeout;

  /// No description provided for @dashboardHint.
  ///
  /// In ru, this message translates to:
  /// **'Path A: PP-001 без прибора на шине — FAULT / не в сети. Управление — только объявленные возможности, с подтверждением.'**
  String get dashboardHint;

  /// No description provided for @noObjects.
  ///
  /// In ru, this message translates to:
  /// **'Нет объектов'**
  String get noObjects;

  /// No description provided for @objectsNote.
  ///
  /// In ru, this message translates to:
  /// **'С2000-ПП = PP-001. Сигнал-10 = S10-001. Сигнал-20 = S20-001. Турникет = TS-001. Сырая шина = RAW-001. ПЦН = PCN-001.'**
  String get objectsNote;

  /// No description provided for @siteBench.
  ///
  /// In ru, this message translates to:
  /// **'Стенд'**
  String get siteBench;

  /// No description provided for @capabilities.
  ///
  /// In ru, this message translates to:
  /// **'возможности'**
  String get capabilities;

  /// No description provided for @address.
  ///
  /// In ru, this message translates to:
  /// **'адрес'**
  String get address;

  /// No description provided for @eventsEmpty.
  ///
  /// In ru, this message translates to:
  /// **'события: []'**
  String get eventsEmpty;

  /// No description provided for @meshTitle.
  ///
  /// In ru, this message translates to:
  /// **'Состояние расширителя (не радиочат)'**
  String get meshTitle;

  /// No description provided for @meshNode.
  ///
  /// In ru, this message translates to:
  /// **'Узел'**
  String get meshNode;

  /// No description provided for @meshRadio.
  ///
  /// In ru, this message translates to:
  /// **'Радио'**
  String get meshRadio;

  /// No description provided for @meshRepeater.
  ///
  /// In ru, this message translates to:
  /// **'Повторитель'**
  String get meshRepeater;

  /// No description provided for @meshRssi.
  ///
  /// In ru, this message translates to:
  /// **'RSSI'**
  String get meshRssi;

  /// No description provided for @meshSnr.
  ///
  /// In ru, this message translates to:
  /// **'SNR'**
  String get meshSnr;

  /// No description provided for @meshRx.
  ///
  /// In ru, this message translates to:
  /// **'RX'**
  String get meshRx;

  /// No description provided for @meshTx.
  ///
  /// In ru, this message translates to:
  /// **'TX'**
  String get meshTx;

  /// No description provided for @meshUptime.
  ///
  /// In ru, this message translates to:
  /// **'Аптайм'**
  String get meshUptime;

  /// No description provided for @meshLastSeen.
  ///
  /// In ru, this message translates to:
  /// **'Последний раз'**
  String get meshLastSeen;

  /// No description provided for @meshProfile.
  ///
  /// In ru, this message translates to:
  /// **'Профиль'**
  String get meshProfile;

  /// No description provided for @meshAdapters.
  ///
  /// In ru, this message translates to:
  /// **'Адаптеры'**
  String get meshAdapters;

  /// No description provided for @meshRs485.
  ///
  /// In ru, this message translates to:
  /// **'RS-485'**
  String get meshRs485;

  /// No description provided for @on.
  ///
  /// In ru, this message translates to:
  /// **'вкл'**
  String get on;

  /// No description provided for @off.
  ///
  /// In ru, this message translates to:
  /// **'выкл'**
  String get off;

  /// No description provided for @bridgeSession.
  ///
  /// In ru, this message translates to:
  /// **'Сессия'**
  String get bridgeSession;

  /// No description provided for @bridgeLevel.
  ///
  /// In ru, this message translates to:
  /// **'Уровень'**
  String get bridgeLevel;

  /// No description provided for @bridgePort.
  ///
  /// In ru, this message translates to:
  /// **'Порт'**
  String get bridgePort;

  /// No description provided for @bridgeOpenLater.
  ///
  /// In ru, this message translates to:
  /// **'BRIDGE_OPEN (не операторское действие)'**
  String get bridgeOpenLater;

  /// No description provided for @bridgeHint.
  ///
  /// In ru, this message translates to:
  /// **'По умолчанию READ_ONLY. Оператор не открывает сессию моста. COMMAND_EXECUTE — экран Управление.'**
  String get bridgeHint;

  /// No description provided for @accessMissing.
  ///
  /// In ru, this message translates to:
  /// **'TS-001 нет в реестре'**
  String get accessMissing;

  /// No description provided for @accessOpen.
  ///
  /// In ru, this message translates to:
  /// **'OPEN'**
  String get accessOpen;

  /// No description provided for @accessLock.
  ///
  /// In ru, this message translates to:
  /// **'LOCK'**
  String get accessLock;

  /// No description provided for @accessHint.
  ///
  /// In ru, this message translates to:
  /// **'OPEN = ACCESS_OPEN, LOCK = ACCESS_LOCK (не COMMAND_EXECUTE). Фото только с диска, не в LoRa. Возможности: {caps}.'**
  String accessHint(String caps);

  /// No description provided for @accessViewerDenied.
  ///
  /// In ru, this message translates to:
  /// **'Наблюдатель не может OPEN/LOCK и не меняет карты. Войдите как оператор или администратор.'**
  String get accessViewerDenied;

  /// No description provided for @accessOperatorHint.
  ///
  /// In ru, this message translates to:
  /// **'OPEN и LOCK — с подтверждением. Карты ACL меняет только администратор.'**
  String get accessOperatorHint;

  /// No description provided for @accessAdminListHint.
  ///
  /// In ru, this message translates to:
  /// **'Полный список карт ACL — только администратор (ACCESS_GET_USERS). Наблюдатель видит число в STATUS.'**
  String get accessAdminListHint;

  /// No description provided for @accessConfirmTitle.
  ///
  /// In ru, this message translates to:
  /// **'Подтвердить OPEN'**
  String get accessConfirmTitle;

  /// No description provided for @accessConfirmBody.
  ///
  /// In ru, this message translates to:
  /// **'TS-001 · ACCESS_OPEN. Будет виден ACK сети, ACK прибора или таймаут.'**
  String get accessConfirmBody;

  /// No description provided for @accessLockConfirmTitle.
  ///
  /// In ru, this message translates to:
  /// **'Подтвердить LOCK'**
  String get accessLockConfirmTitle;

  /// No description provided for @accessLockConfirmBody.
  ///
  /// In ru, this message translates to:
  /// **'TS-001 · ACCESS_LOCK. Будет виден ACK сети, ACK прибора или таймаут.'**
  String get accessLockConfirmBody;

  /// No description provided for @accessEmployees.
  ///
  /// In ru, this message translates to:
  /// **'Сотрудники / карты'**
  String get accessEmployees;

  /// No description provided for @accessAddEmployee.
  ///
  /// In ru, this message translates to:
  /// **'Добавить'**
  String get accessAddEmployee;

  /// No description provided for @accessEditEmployee.
  ///
  /// In ru, this message translates to:
  /// **'Карта'**
  String get accessEditEmployee;

  /// No description provided for @accessName.
  ///
  /// In ru, this message translates to:
  /// **'Имя'**
  String get accessName;

  /// No description provided for @accessEmployeeId.
  ///
  /// In ru, this message translates to:
  /// **'employee_id'**
  String get accessEmployeeId;

  /// No description provided for @accessUid.
  ///
  /// In ru, this message translates to:
  /// **'UID (hex)'**
  String get accessUid;

  /// No description provided for @accessUidRequired.
  ///
  /// In ru, this message translates to:
  /// **'Нужен UID карты'**
  String get accessUidRequired;

  /// No description provided for @accessEnabled.
  ///
  /// In ru, this message translates to:
  /// **'Разрешена'**
  String get accessEnabled;

  /// No description provided for @accessDisabled.
  ///
  /// In ru, this message translates to:
  /// **'отключена'**
  String get accessDisabled;

  /// No description provided for @accessPhotoPath.
  ///
  /// In ru, this message translates to:
  /// **'Фото (путь на диске)'**
  String get accessPhotoPath;

  /// No description provided for @accessPhotoHint.
  ///
  /// In ru, this message translates to:
  /// **'Только локальный файл. Байты фото не уходят на узел / LoRa / OLED.'**
  String get accessPhotoHint;

  /// No description provided for @accessSave.
  ///
  /// In ru, this message translates to:
  /// **'Сохранить'**
  String get accessSave;

  /// No description provided for @accessRemove.
  ///
  /// In ru, this message translates to:
  /// **'Удалить'**
  String get accessRemove;

  /// No description provided for @accessEmptyEmployees.
  ///
  /// In ru, this message translates to:
  /// **'На узле нет карт (пустой ACL = DENY).'**
  String get accessEmptyEmployees;

  /// No description provided for @accessEvents.
  ///
  /// In ru, this message translates to:
  /// **'События турникета TS-001'**
  String get accessEvents;

  /// No description provided for @accessBusy.
  ///
  /// In ru, this message translates to:
  /// **'Узел BUSY (ACL полон, cap 64). Карта не добавлена.'**
  String get accessBusy;

  /// No description provided for @accessStatusLine.
  ///
  /// In ru, this message translates to:
  /// **'Состояние {state} · users {users}'**
  String accessStatusLine(String state, int users);

  /// No description provided for @settingsRole.
  ///
  /// In ru, this message translates to:
  /// **'Роль'**
  String get settingsRole;

  /// No description provided for @settingsTransport.
  ///
  /// In ru, this message translates to:
  /// **'Транспорт'**
  String get settingsTransport;

  /// No description provided for @settingsServer.
  ///
  /// In ru, this message translates to:
  /// **'URL сервера'**
  String get settingsServer;

  /// No description provided for @settingsServerNone.
  ///
  /// In ru, this message translates to:
  /// **'(нет — LAN REST позже)'**
  String get settingsServerNone;

  /// No description provided for @settingsNode.
  ///
  /// In ru, this message translates to:
  /// **'Узел'**
  String get settingsNode;

  /// No description provided for @settingsPort.
  ///
  /// In ru, this message translates to:
  /// **'Протокол порта / скорость'**
  String get settingsPort;

  /// No description provided for @settingsPortHint.
  ///
  /// In ru, this message translates to:
  /// **'{protocol} {baud} 8N1 (только просмотр — GPIO в прошивке)'**
  String settingsPortHint(String protocol, int baud);

  /// No description provided for @settingsPortPicker.
  ///
  /// In ru, this message translates to:
  /// **'Порт / устройство'**
  String get settingsPortPicker;

  /// No description provided for @settingsFixture.
  ///
  /// In ru, this message translates to:
  /// **'fixture (JSON в памяти)'**
  String get settingsFixture;

  /// No description provided for @settingsRefresh.
  ///
  /// In ru, this message translates to:
  /// **'Обновить'**
  String get settingsRefresh;

  /// No description provided for @settingsBlePin.
  ///
  /// In ru, this message translates to:
  /// **'BLE PIN MeshCore: 123456 (системный диалог Android).'**
  String get settingsBlePin;

  /// No description provided for @settingsBleGap.
  ///
  /// In ru, this message translates to:
  /// **'BLE NUS: байт открывающей скобки JSON идёт в MESH 485, не в Companion. Не оборачивать JSON в кадры 0x3c/0x3e. USB-OTG — запасной путь, если пары нет.'**
  String get settingsBleGap;

  /// No description provided for @filterDevice.
  ///
  /// In ru, this message translates to:
  /// **'Прибор'**
  String get filterDevice;

  /// No description provided for @filterType.
  ///
  /// In ru, this message translates to:
  /// **'Тип'**
  String get filterType;

  /// No description provided for @filterAll.
  ///
  /// In ru, this message translates to:
  /// **'все'**
  String get filterAll;

  /// No description provided for @connectFailed.
  ///
  /// In ru, this message translates to:
  /// **'Нет порта / нет ответа — показан fixture.'**
  String get connectFailed;

  /// No description provided for @seconds.
  ///
  /// In ru, this message translates to:
  /// **'{n} с'**
  String seconds(int n);

  /// No description provided for @controlEmpty.
  ///
  /// In ru, this message translates to:
  /// **'Нет разрешённых команд. Реле только если адаптер объявил CONTROL_RELAY и конфиг панели разрешил именованную точку (R1…).'**
  String get controlEmpty;

  /// No description provided for @controlHint.
  ///
  /// In ru, this message translates to:
  /// **'Нет команды «отключить пожарную тревогу» / «снять все». OPEN турникета — экран Доступ (ACCESS_OPEN). Нужно подтверждение.'**
  String get controlHint;

  /// No description provided for @controlViewerDenied.
  ///
  /// In ru, this message translates to:
  /// **'Наблюдатель не может выполнять COMMAND_EXECUTE. Войдите как оператор или администратор.'**
  String get controlViewerDenied;

  /// No description provided for @controlConfirmTitle.
  ///
  /// In ru, this message translates to:
  /// **'Подтвердить управление'**
  String get controlConfirmTitle;

  /// No description provided for @controlConfirmBody.
  ///
  /// In ru, this message translates to:
  /// **'{device} · {cap} · {point} · {action}. Будет виден ACK сети, ACK прибора или таймаут.'**
  String controlConfirmBody(
    String device,
    String cap,
    String point,
    String action,
  );

  /// No description provided for @controlConfirm.
  ///
  /// In ru, this message translates to:
  /// **'Выполнить'**
  String get controlConfirm;

  /// No description provided for @controlCancel.
  ///
  /// In ru, this message translates to:
  /// **'Отмена'**
  String get controlCancel;

  /// No description provided for @controlLastResult.
  ///
  /// In ru, this message translates to:
  /// **'Последний результат'**
  String get controlLastResult;

  /// No description provided for @controlOn.
  ///
  /// In ru, this message translates to:
  /// **'ВКЛ'**
  String get controlOn;

  /// No description provided for @controlOff.
  ///
  /// In ru, this message translates to:
  /// **'ВЫКЛ'**
  String get controlOff;
}

class _AppLocalizationsDelegate
    extends LocalizationsDelegate<AppLocalizations> {
  const _AppLocalizationsDelegate();

  @override
  Future<AppLocalizations> load(Locale locale) {
    return SynchronousFuture<AppLocalizations>(lookupAppLocalizations(locale));
  }

  @override
  bool isSupported(Locale locale) =>
      <String>['ru'].contains(locale.languageCode);

  @override
  bool shouldReload(_AppLocalizationsDelegate old) => false;
}

AppLocalizations lookupAppLocalizations(Locale locale) {
  // Lookup logic when only language code is specified.
  switch (locale.languageCode) {
    case 'ru':
      return AppLocalizationsRu();
  }

  throw FlutterError(
    'AppLocalizations.delegate failed to load unsupported locale "$locale". This is likely '
    'an issue with the localizations generation tool. Please file an issue '
    'on GitHub with a reproducible sample app and the gen-l10n configuration '
    'that was used.',
  );
}
