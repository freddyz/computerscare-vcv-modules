import copy
import importlib.util
import json
from pathlib import Path
import unittest
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('port_rules', ROOT / 'scripts/generate-beekapoo-port-rules.py')
rules = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rules)


class PortRulesTest(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((rules.SOURCE / 'Fundamental.json').read_text())

    def test_all_bundled_rules_and_reproducible_output(self):
        rules.validate(self.data)
        self.assertEqual(rules.generate(), rules.TARGET.read_text())

    def test_preferred_mix_and_readable_labels(self):
        mixer = self.data['modules']['VCMixer']
        self.assertEqual(mixer['preferredOutputs'], [0])
        self.assertEqual(mixer['outputs']['0']['label'], 'MIX')
        self.assertEqual(mixer['inputs']['6']['requiresOutputs'], [[0, 2]])
        self.assertEqual(mixer['inputs']['6']['requiresInputs'], [[2]])

    def test_labels_do_not_affect_compiled_rules(self):
        original = rules.generate()
        source = rules.SOURCE
        with tempfile.TemporaryDirectory() as folder:
            try:
                rules.SOURCE = Path(folder)
                for path in source.glob('*.json'):
                    data = json.loads(path.read_text())
                    for module in data['modules'].values():
                        for side in ('inputs', 'outputs'):
                            for port in module.get(side, {}).values():
                                port.pop('label', None)
                    (rules.SOURCE / path.name).write_text(json.dumps(data))
                self.assertEqual(original, rules.generate())
            finally:
                rules.SOURCE = source

    def test_unknown_ids_empty_groups_and_type_fields_rejected(self):
        for value in ([[99]], [[]], [1], [[True]], [[0, 0]]):
            data = copy.deepcopy(self.data)
            data['modules']['VCO']['inputs']['3']['requiresOutputs'] = value
            with self.assertRaises(ValueError): rules.validate(data)
        self.data['modules']['VCO']['outputs']['0']['signal'] = 'sound'
        with self.assertRaises(ValueError): rules.validate(self.data)

    def test_multiple_and_or_groups(self):
        self.data['modules']['Delay']['outputs']['0']['requiresInputs'] = [[4], [0, 1]]
        rules.validate(self.data)

    def test_circular_dependencies_rejected(self):
        self.data['modules']['VCO']['inputs']['0']['requiresInputs'] = [[1]]
        self.data['modules']['VCO']['inputs']['1']['requiresInputs'] = [[0]]
        with self.assertRaises(ValueError): rules.validate(self.data)

    def test_unknown_preferred_output(self):
        self.data['modules']['VCMixer']['preferredOutputs'] = [50]
        with self.assertRaises(ValueError): rules.validate(self.data)

    def test_bad_version_and_unknown_fields(self):
        self.data['version'] = 2
        with self.assertRaises(ValueError): rules.validate(self.data)


if __name__ == '__main__': unittest.main()
