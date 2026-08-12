#include "runtime_log_dialog.h"

#include "ui_scale_utils.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QTextCursor>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <memory>

namespace {

QDateTime ParseLineTime(const QString& line)
{
    int index = line.indexOf(QStringLiteral("20"));
    while (index >= 0 && index + 19 <= line.size()) {
        const QString candidate = line.mid(index, 19);
        QDateTime parsed = QDateTime::fromString(candidate, Qt::ISODate);
        if (!parsed.isValid()) {
            parsed = QDateTime::fromString(candidate, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        }
        if (parsed.isValid()) {
            return parsed;
        }
        index = line.indexOf(QStringLiteral("20"), index + 1);
    }
    return QDateTime();
}

void AddLogFile(QComboBox* log_selector, const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return;
    }
    for (int i = 0; i < log_selector->count(); ++i) {
        if (log_selector->itemData(i).toString() == info.absoluteFilePath()) {
            return;
        }
    }
    const QString label = QStringLiteral("%1  %2")
                              .arg(info.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
                              .arg(info.absoluteFilePath());
    log_selector->addItem(label, info.absoluteFilePath());
}

void AddLogDir(QComboBox* log_selector, const QDir& dir)
{
    const QFileInfoList logs = dir.entryInfoList({ QStringLiteral("*.log") }, QDir::Files, QDir::Time);
    for (const QFileInfo& info : logs) {
        AddLogFile(log_selector, info.absoluteFilePath());
    }
}

} // namespace

void ShowRuntimeLogDialog(QWidget* parent, double ui_scale)
{
    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QString::fromUtf8("运行日志"));
    dialog->resize(ScaleSize(1080, 680, ui_scale));

    auto* layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(ScaleMargins(14, 14, 14, 14, ui_scale));
    layout->setSpacing(ScalePx(8, ui_scale));

    auto* selector_row = new QHBoxLayout();
    selector_row->setSpacing(ScalePx(8, ui_scale));
    selector_row->addWidget(new QLabel(QString::fromUtf8("日志文件"), dialog));
    auto* log_selector = new QComboBox(dialog);
    log_selector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selector_row->addWidget(log_selector, 1);
    layout->addLayout(selector_row);

    auto* filter_row = new QHBoxLayout();
    filter_row->setSpacing(ScalePx(8, ui_scale));
    auto* search_edit = new QLineEdit(dialog);
    search_edit->setPlaceholderText(QString::fromUtf8("搜索关键词，例如 error / warning / PLC / camera"));
    auto* level_selector = new QComboBox(dialog);
    level_selector->addItem(QString::fromUtf8("全部"), QString());
    level_selector->addItem(QStringLiteral("ERROR"), QStringLiteral("error|failed|fail|失败"));
    level_selector->addItem(QStringLiteral("WARNING"), QStringLiteral("warning|warn|警告"));
    level_selector->addItem(QStringLiteral("PLC"), QStringLiteral("plc|modbus|register|d500|d502|d506"));
    level_selector->addItem(QString::fromUtf8("相机"), QStringLiteral("camera|hik|frame|相机"));
    level_selector->addItem(QString::fromUtf8("模型"), QStringLiteral("model|openvino|obb|seg|模型"));
    auto* from_time = new QDateTimeEdit(dialog);
    auto* to_time = new QDateTimeEdit(dialog);
    const QDateTime now = QDateTime::currentDateTime();
    from_time->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    to_time->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    from_time->setCalendarPopup(true);
    to_time->setCalendarPopup(true);
    from_time->setDateTime(now.addDays(-7));
    to_time->setDateTime(now.addDays(1));
    filter_row->addWidget(search_edit, 2);
    filter_row->addWidget(level_selector);
    filter_row->addWidget(new QLabel(QString::fromUtf8("从"), dialog));
    filter_row->addWidget(from_time);
    filter_row->addWidget(new QLabel(QString::fromUtf8("到"), dialog));
    filter_row->addWidget(to_time);
    layout->addLayout(filter_row);

    auto* log_location_label = new QLabel(dialog);
    log_location_label->setWordWrap(true);
    log_location_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(log_location_label);

    auto* log_view = new QTextEdit(dialog);
    log_view->setReadOnly(true);
    log_view->setLineWrapMode(QTextEdit::NoWrap);
    log_view->setObjectName("logTextView");
    layout->addWidget(log_view, 1);

    auto* page_row = new QHBoxLayout();
    page_row->setSpacing(ScalePx(8, ui_scale));
    auto* prev_button = new QPushButton(QString::fromUtf8("上一页"), dialog);
    auto* next_button = new QPushButton(QString::fromUtf8("下一页"), dialog);
    auto* latest_button = new QPushButton(QString::fromUtf8("最新"), dialog);
    auto* page_label = new QLabel(dialog);
    auto* close_button = new QPushButton(QString::fromUtf8("关闭"), dialog);
    page_row->addWidget(prev_button);
    page_row->addWidget(next_button);
    page_row->addWidget(latest_button);
    page_row->addWidget(page_label, 1);
    page_row->addWidget(close_button);
    layout->addLayout(page_row);

    dialog->setStyleSheet(QString(
        "QDialog { background: #0f141a; color: #e6edf7; }"
        "QLabel { color: #dbe7f1; font-size: %1px; font-weight: 600; }"
        "QComboBox, QDateTimeEdit, QLineEdit { background: #181f29; color: #f4f8fc; border: 1px solid #354b60; border-radius: %2px; padding: %3px %4px; font-size: %1px; }"
        "QTextEdit#logTextView { background: #080d12; color: #dce7f1; border: 1px solid #27303d; border-radius: %5px; font-family: Consolas, 'Courier New'; font-size: %6px; }"
        "QPushButton { background: #2a4365; color: #ecf4fa; border: 1px solid #36546b; border-radius: %7px; padding: %8px %9px; font-size: %1px; font-weight: 600; }")
        .arg(ScalePx(13, ui_scale))
        .arg(ScalePx(6, ui_scale))
        .arg(ScalePx(6, ui_scale))
        .arg(ScalePx(8, ui_scale))
        .arg(ScalePx(8, ui_scale))
        .arg(ScalePx(12, ui_scale))
        .arg(ScalePx(7, ui_scale))
        .arg(ScalePx(6, ui_scale))
        .arg(ScalePx(12, ui_scale)));

    const QDir app_log_dir(QCoreApplication::applicationDirPath() + QStringLiteral("/logs"));
    const QDir cwd_log_dir(QDir::current().filePath(QStringLiteral("logs")));
    const QString env_log_file = QString::fromLocal8Bit(qgetenv("TANKEYE_LOG_FILE")).trimmed();
    if (!env_log_file.isEmpty()) {
        AddLogFile(log_selector, env_log_file);
    }
    AddLogDir(log_selector, app_log_dir);
    AddLogDir(log_selector, cwd_log_dir);
    log_location_label->setText(QString::fromUtf8("日志位置：%1；%2")
                                    .arg(app_log_dir.absolutePath(), cwd_log_dir.absolutePath()));

    constexpr int kPageSize = 1000;
    constexpr qint64 kMaxReadBytes = 4 * 1024 * 1024;
    auto page_index = std::make_shared<int>(0);
    auto latest_mode = std::make_shared<bool>(true);
    const auto refresh_log = [=]() {
        const QString path = log_selector->currentData().toString();
        if (path.isEmpty()) {
            log_view->setPlainText(QString::fromUtf8("暂无日志。\n%1").arg(log_location_label->text()));
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            log_view->setPlainText(QString::fromUtf8("日志打开失败：%1\n原因：%2").arg(path, file.errorString()));
            return;
        }
        if (file.size() > kMaxReadBytes) {
            file.seek(file.size() - kMaxReadBytes);
            file.readLine();
        }
        const QStringList raw_lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'));
        const QString keyword = search_edit->text().trimmed();
        const QStringList level_terms = level_selector->currentData().toString().split(QLatin1Char('|'), Qt::SkipEmptyParts);
        QStringList filtered;
        filtered.reserve(raw_lines.size());
        for (const QString& raw_line : raw_lines) {
            const QString line = raw_line.trimmed();
            if (line.isEmpty()) {
                continue;
            }
            if (!keyword.isEmpty() && !line.contains(keyword, Qt::CaseInsensitive)) {
                continue;
            }
            bool level_match = level_terms.isEmpty();
            for (const QString& term : level_terms) {
                if (line.contains(term, Qt::CaseInsensitive)) {
                    level_match = true;
                    break;
                }
            }
            if (!level_match) {
                continue;
            }
            const QDateTime line_time = ParseLineTime(line);
            if (line_time.isValid() && (line_time < from_time->dateTime() || line_time > to_time->dateTime())) {
                continue;
            }
            filtered.push_back(line);
        }
        const int page_count = qMax(1, (filtered.size() + kPageSize - 1) / kPageSize);
        if (*latest_mode) {
            *page_index = page_count - 1;
        }
        *page_index = qBound(0, *page_index, page_count - 1);
        log_view->setPlainText(filtered.mid((*page_index) * kPageSize, kPageSize).join(QStringLiteral("\n")));
        log_view->moveCursor(QTextCursor::End);
        page_label->setText(QString::fromUtf8("第 %1/%2 页，每页 %3 行；匹配 %4 行；当前：%5")
                                .arg(*page_index + 1)
                                .arg(page_count)
                                .arg(kPageSize)
                                .arg(filtered.size())
                                .arg(QFileInfo(path).fileName()));
    };

    QObject::connect(log_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [=](int) {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(search_edit, &QLineEdit::textChanged, dialog, [=]() {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(level_selector, QOverload<int>::of(&QComboBox::currentIndexChanged), dialog, [=](int) {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(from_time, &QDateTimeEdit::dateTimeChanged, dialog, [=](const QDateTime&) {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(to_time, &QDateTimeEdit::dateTimeChanged, dialog, [=](const QDateTime&) {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(prev_button, &QPushButton::clicked, dialog, [=]() {
        *latest_mode = false;
        --(*page_index);
        refresh_log();
    });
    QObject::connect(next_button, &QPushButton::clicked, dialog, [=]() {
        *latest_mode = false;
        ++(*page_index);
        refresh_log();
    });
    QObject::connect(latest_button, &QPushButton::clicked, dialog, [=]() {
        *latest_mode = true;
        refresh_log();
    });
    QObject::connect(close_button, &QPushButton::clicked, dialog, &QDialog::accept);

    refresh_log();
    auto* auto_refresh_timer = new QTimer(dialog);
    auto_refresh_timer->setInterval(500);
    QObject::connect(auto_refresh_timer, &QTimer::timeout, dialog, refresh_log);
    auto_refresh_timer->start();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
