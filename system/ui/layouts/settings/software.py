class SoftwareSettings:
    def __init__(self, parent):
        self.parent = parent
        self.settings = {
            "auto_update": True,
            "theme": "light",
            "language": "en",
            "notifications": True,
        }

    def toggle_auto_update(self):
        self.settings["auto_update"] = not self.settings["auto_update"]

    def set_theme(self, theme):
        if theme in ["light", "dark"]:
            self.settings["theme"] = theme

    def set_language(self, language):
        if language in ["en", "es", "fr", "de"]:
            self.settings["language"] = language

    def toggle_notifications(self):
        self.settings["notifications"] = not self.settings["notifications"]