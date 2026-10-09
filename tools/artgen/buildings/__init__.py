"""Building generators, one module per building family."""
from . import placeholder, fixed, housing, painted


def generate():
    out = []
    out += placeholder.generate()
    out += fixed.generate()
    out += housing.generate()
    out += painted.generate()
    return out
