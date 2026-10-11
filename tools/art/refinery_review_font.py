"""Portable review-sheet fonts; retain the original font when available."""
from PIL import ImageFont


def load_review_font(preferred, size):
    bold = 'bold' in str(preferred).lower()
    alternatives = (
        preferred,
        'arialbd.ttf' if bold else 'arial.ttf',
        'DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf',
    )
    for candidate in alternatives:
        try:
            return ImageFont.truetype(candidate, size)
        except OSError:
            pass
    return ImageFont.load_default(size=size)
