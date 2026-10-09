"""Building generators, one module per building family."""
from . import placeholder


def generate():
    out = []
    out += placeholder.generate()
    return out
