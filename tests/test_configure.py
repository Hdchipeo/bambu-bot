import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("configure", Path(__file__).resolve().parents[1] / "scripts/configure.py")
configure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configure)


class ConfigureTests(unittest.TestCase):
    def test_demo_needs_no_credentials(self):
        result = configure.generate({"demo": True}, Path("."))
        self.assertIn("#define BAMBU_DEMO true", result)

    def test_string_false_is_rejected(self):
        with self.assertRaises(ValueError):
            configure.generate({"demo": "false"}, Path("."))

    def test_live_settings_are_required(self):
        with self.assertRaises(ValueError):
            configure.generate({"demo": False}, Path("."))

    def test_mqtt_topic_injection_rejected(self):
        with self.assertRaises(ValueError):
            configure.generate({"demo": False, "host": "192.168.1.5", "serial": "abc/+/report", "access_code": "12345678"}, Path("."))

    def test_invalid_server_url_rejected(self):
        with self.assertRaises(ValueError):
            configure.generate({"ota_url": "file:///tmp/foo"}, Path("."))

    def test_unknown_keys_rejected(self):
        with self.assertRaises(ValueError):
            configure.generate({"servo_gpio": 18}, Path("."))

    def test_notification_and_wiki_options(self):
        result=configure.generate({"voice_notifications":False,"wiki_url":"http://192.168.1.9:8765/","wiki_token":"local-test"},Path("."))
        self.assertIn("#define BAMBU_VOICE_NOTIFICATIONS false",result)
        self.assertIn('BAMBU_WIKI_URL "http://192.168.1.9:8765"',result)
    def test_wiki_credentials_and_public_http_rejected(self):
        for data in [{"voice_notifications":"true"},{"wiki_url":"http://192.168.1.9:8765"},
                     {"wiki_url":"http://8.8.8.8","wiki_token":"test"},{"wiki_url":"https://user:pass@example.com"},
                     {"wiki_token":"bad\r\nheader"}]:
            with self.assertRaises(ValueError):configure.generate(data,Path("."))
