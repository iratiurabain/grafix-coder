#include "GrafixSettingsLoader.h"

GrafixSettingsLoader::GrafixSettingsLoader(const GrafixParticipant &participant) {
    Init(participant.GetProject()->GetProjectSettingsPath(), Consts::ACTIVE_CONFIGURATION());
}

GrafixSettingsLoader::GrafixSettingsLoader(const GrafixProject &project) {
    Init(project.GetProjectSettingsPath(), Consts::ACTIVE_CONFIGURATION());
}


GrafixSettingsLoader::GrafixSettingsLoader(const GrafixProject &project, const GrafixConfiguration configuration) {
    Init(project.GetProjectSettingsPath(), configuration);
}

GrafixSettingsLoader::GrafixSettingsLoader(QString path, const GrafixConfiguration configuration) {
    Init(path, configuration);
}

GrafixSettingsLoader::GrafixSettingsLoader(const QString path) {
    Init(path, Consts::ACTIVE_CONFIGURATION());
}


void GrafixSettingsLoader::Init(const QString path, const GrafixConfiguration configuration) {
    this->_path = path;
    this->_configuration = configuration;
}

QVariant GrafixSettingsLoader::LoadSetting(QString setting) {
    QSettings settings(_path, QSettings::IniFormat);
    QString key = setting + "_" + QString::number(this->_configuration.second);
    QVariant value = settings.value(key);
    if (value.isNull()) {
        return Consts::DefaultSetting(setting);
    }
    return value;
}

int GrafixSettingsLoader::LoadSliderSetting(QString setting) {
    return Consts::GetSliderValue(setting, LoadSetting(setting).toDouble());
}

void GrafixSettingsLoader::SetSetting(const QString setting, const QVariant value) {
    QString key = setting + "_" + QString::number(this->_configuration.second);
    QSettings settings(_path, QSettings::IniFormat);
    settings.setValue(key, value);
}
