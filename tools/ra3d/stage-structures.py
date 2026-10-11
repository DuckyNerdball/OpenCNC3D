#!/usr/bin/env python3
"""Copy the verified RA building files into a module without touching other art."""
import argparse
from pathlib import Path
import shutil


def stage(module):
    source = Path(__file__).resolve().parents[2] / 'game/ebld_ra'
    models = sorted(source.glob('*.ebld'))
    if not models:
        raise RuntimeError(f'no RA building exports in {source}')
    target = Path(module) / 'ebld'
    target.mkdir(parents=True, exist_ok=True)
    for model in models:
        shutil.copy2(model, target / model.name)
    shutil.copy2(source / 'manifest.json', target / 'manifest.json')
    print(f'RA structures staged: {len(models)} intact models in {target}')


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('module', type=Path)
    stage(ap.parse_args().module)
