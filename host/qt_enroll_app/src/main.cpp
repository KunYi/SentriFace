#include <QApplication>
#include <QTranslator>

#include <QStringList>

#include "qt_enroll_app/frame_source.hpp"
#include "qt_enroll_app/main_window.hpp"
#include "qt_enroll_app/observation_source.hpp"

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("sentriface_enroll_app"));
  app.setOrganizationName(QStringLiteral("SentriFace"));

  QTranslator translator;
  translator.load(QLocale(), QStringLiteral("sentriface_qt_enroll"), QStringLiteral("_"), QStringLiteral(":/i18n"));
  app.installTranslator(&translator);

  const sentriface::host::PreviewFrameSourceConfig source_config =
      sentriface::host::LoadPreviewFrameSourceConfigFromArgs(argc, argv);
  const sentriface::host::ObservationSourceConfig observation_config =
      sentriface::host::LoadObservationSourceConfigFromArgs(argc, argv);
  sentriface::host::MainWindow window(source_config, observation_config);

  const QStringList arguments = QCoreApplication::arguments();
  QString user_id;
  QString user_name;
  bool auto_start = false;
  bool auto_close_on_review = false;
  for (const QString& arg : arguments) {
    if (arg.startsWith(QStringLiteral("--user-id="))) {
      user_id = arg.section(QChar('='), 1);
    } else if (arg.startsWith(QStringLiteral("--user-name="))) {
      user_name = arg.section(QChar('='), 1);
    } else if (arg == QStringLiteral("--auto-start")) {
      auto_start = true;
    } else if (arg == QStringLiteral("--auto-close-on-review")) {
      auto_close_on_review = true;
    }
  }

  if (!user_id.isEmpty() || !user_name.isEmpty()) {
    window.SetEnrollmentIdentity(user_id, user_name);
  }
  window.SetAutoCloseOnReview(auto_close_on_review);
  window.show();
  if (auto_start) {
    window.ScheduleAutoStart();
  }
  return app.exec();
}
