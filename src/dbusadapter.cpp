#include "dbusadapter.h"

CarHotspotDBusAdapter::CarHotspotDBusAdapter(bool isDaemon, QObject *parent)
    : QObject(parent)
    , m_isDaemon(isDaemon)
{
}

void CarHotspotDBusAdapter::openApp()
{
    emit requestActivate();
}

void CarHotspotDBusAdapter::activate()
{
    emit requestActivate();
}
