#pragma once
#include <QDialog>
#include <QPointer>
#include <QSignalMapper>
#include <QStringList>
#include <QHBoxLayout>
#include <dlfcn.h>
#include <new>
#include <stdexcept>

// Exact-firmware native UI. Standard Qt mouse-driven widgets do not receive
// Nickel's custom TapGesture. Use its dialog factory and registered menu rows.
struct NativeUi {
    QDialog *(*create)(QWidget *);
    void (*title)(QDialog *, const QString &);
    void (*body)(QDialog *, const QString &);
    void (*acceptText)(QDialog *, const QString &);
    void (*rejectText)(QDialog *, const QString &);
    void (*rejectVisible)(QDialog *, bool);
    QWidget *(*acceptButton)(const QDialog *);
    void (*addWidget)(QDialog *, QWidget *);
    void (*itemCtor)(QWidget *, QWidget *, bool, bool);
    void (*itemText)(QWidget *, const QString &);
    void (*itemGestures)(QWidget *);
    void (*showOK)(const QString &, const QString &);

    template<class T> void bind(T &target, const char *symbol) {
        target = reinterpret_cast<T>(dlsym(RTLD_DEFAULT, symbol));
        if (!target) throw std::runtime_error("Native touch UI is unavailable.");
    }
    NativeUi() {
        bind(create, "_ZN25ConfirmationDialogFactory21getConfirmationDialogEP7QWidget");
        bind(title, "_ZN18ConfirmationDialog8setTitleERK7QString");
        bind(body, "_ZN18ConfirmationDialog7setTextERK7QString");
        bind(acceptText, "_ZN18ConfirmationDialog19setAcceptButtonTextERK7QString");
        bind(rejectText, "_ZN18ConfirmationDialog19setRejectButtonTextERK7QString");
        bind(rejectVisible, "_ZN18ConfirmationDialog22setRejectButtonVisibleEb");
        bind(acceptButton, "_ZNK18ConfirmationDialog12acceptButtonEv");
        bind(addWidget, "_ZN18ConfirmationDialog9addWidgetEP7QWidget");
        bind(itemCtor, "_ZN12MenuTextItemC1EP7QWidgetbb");
        bind(itemText, "_ZN12MenuTextItem7setTextERK7QString");
        bind(itemGestures, "_ZN12MenuTextItem22registerForTapGesturesEv");
        bind(showOK, "_ZN25ConfirmationDialogFactory12showOKDialogERK7QStringS2_");
    }
    struct OwnedDialog {
        QPointer<QDialog> value;
        explicit OwnedDialog(QDialog *dialog) : value(dialog) {
            if (!dialog) throw std::runtime_error("Native dialog creation failed.");
            dialog->setAttribute(Qt::WA_DeleteOnClose, false);
        }
        ~OwnedDialog() { if (value) delete value.data(); }
    };
    QWidget *tapItem(QWidget *parent, const QString &label) {
        // Native createMenuTextItem allocates 0x60 bytes on the pinned firmware.
        void *memory = ::operator new(0x60);
        QWidget *item = static_cast<QWidget *>(memory);
        try { itemCtor(item, parent, false, true); }
        catch (...) { ::operator delete(memory); throw; }
        itemText(item, label);
        itemGestures(item);
        item->setMinimumHeight(qMax(72, item->fontMetrics().height() * 2));
        return item;
    }
    int choose(QWidget *parent, const QString &heading, const QStringList &labels, int contentCount = -1) {
        OwnedDialog owned(create(parent));
        QDialog *dialog = owned.value.data();
        title(dialog, parent->fontMetrics().elidedText(heading, Qt::ElideMiddle, qMax(240, parent->width() - 120)));
        acceptButton(dialog)->hide();
        rejectText(dialog, "Cancel");
        rejectVisible(dialog, true);
        QSignalMapper *mapper = new QSignalMapper(dialog);
        QObject::connect(mapper, static_cast<void(QSignalMapper::*)(int)>(&QSignalMapper::mapped),
            dialog, [dialog](int index) { dialog->done(1000 + index); });
        QWidget *navigation = nullptr;
        QHBoxLayout *navigationLayout = nullptr;
        if (contentCount >= 0 && contentCount < labels.size()) {
            navigation = new QWidget(dialog);
            navigationLayout = new QHBoxLayout(navigation);
            navigationLayout->setContentsMargins(0, 0, 0, 0);
        }
        for (int i = 0; i < labels.size(); ++i) {
            const bool isNavigation = navigation && i >= contentCount;
            const int width = qMax(240, parent->width() - 120);
            const QString label = isNavigation ? labels.at(i)
                : parent->fontMetrics().elidedText(labels.at(i), Qt::ElideMiddle, width);
            QWidget *item = tapItem(isNavigation ? navigation : dialog, label);
            mapper->setMapping(item, i);
            if (!QObject::connect(item, SIGNAL(tapped(bool)), mapper, SLOT(map())))
                throw std::runtime_error("Cannot connect native tap handler.");
            if (isNavigation) navigationLayout->addWidget(item, 1);
            else addWidget(dialog, item);
        }
        if (navigation) addWidget(dialog, navigation);
        const int result = dialog->exec();
        return result >= 1000 && result < 1000 + labels.size() ? result - 1000 : -1;
    }
    bool confirm(QWidget *parent, const QString &notebook) {
        OwnedDialog owned(create(parent));
        QDialog *dialog = owned.value.data();
        title(dialog, "Link notebook");
        body(dialog, "Link this book to <b>" + notebook.toHtmlEscaped() + "</b>?");
        acceptText(dialog, "Link");
        rejectText(dialog, "Cancel");
        rejectVisible(dialog, true);
        return dialog->exec() == QDialog::Accepted;
    }
};
