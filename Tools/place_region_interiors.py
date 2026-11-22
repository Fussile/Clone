"""Walkable site layouts for Crossroads' regional plots."""
from dataclasses import dataclass


@dataclass
class Site:
    box: object
    door: object
    ramp: object
    cache: object
    encounter: object
    sign: object
    window: object
    bed: object
    furniture: object
    shell: object
    pair: object
    palette: dict
    key: str
    name: str
    x: float
    y: float
    z: float
    width: float
    depth: float
    wall: str

    def part(self, key, label, dx, dy, z, w, d, h, finish, **options):
        self.box(f"{self.key}_{key}", label, self.x+dx, self.y+dy, self.z+z,
                 w, d, h, material=self.palette[finish], **options)

    def leaf(self, key, label, dx, dy, z, width, height=2.25, **options):
        self.door(f"{self.key}_{key}", label, self.x+dx, self.y+dy, self.z+z, width, height, **options)

    def partition(self, key, y, span, opening, width, height, z=0, label="Interior door",
                  finish="timber"):
        start, end = span
        left, right = opening-width/2, opening+width/2
        for i, (a, b) in enumerate(((start, left), (right, end))):
            if b>a:
                self.part(f"{key}_{i}", "Interior partition", (a+b)/2, y, z, b-a, .22, height, self.wall)
        self.part(key+"_header", "Door lintel", opening, y, z+2.25, width, .22,
                  height-2.25, self.wall)
        self.leaf(key+"_leaf", label, opening, y, z, width, finish=finish)

    def building(self, suffix, width, depth, height, entry, *, dx=0, dy=0, finish="timber"):
        x, y = self.x+dx, self.y+dy
        front = self.shell(self.box, self.key+suffix, x, y, self.z, width, depth, height,
                           entry, self.palette[self.wall], self.palette["dark_trim"],
                           clear_height=3.1 if entry>3.3 else 2.35)
        door_key = self.key+suffix+"_entry"
        if entry>3.3:
            self.door(door_key, "Loading shutter", x, front-.09, self.z,
                      entry, 3.1, roll_up=True, finish="petrol")
        elif entry>=2:
            self.pair(self.door, door_key, "Entrance doors", x, front-.04, self.z, entry, 2.35, finish)
        else:
            self.door(door_key, "Entrance door", x, front-.04, self.z, entry, 2.35, finish=finish)
        return front

    def label(self, front):
        self.part("fascia", "Signboard", 0, front-self.y-.16, 2.61,
                  min(self.width*.78, 15), .26, .58, "dark_trim")
        self.sign(self.key+"_label", self.name.upper(), self.x, front-.31, self.z+3.04, size=34)

    def storage(self, key, label, dx, dy, z=0, **loot):
        self.cache(f"{self.key}_{key}", label, self.x+dx, self.y+dy, self.z+z, **loot)

    def walker(self, key, dx, dy, z=0, runner=False):
        self.encounter(f"{self.key}_{key}", f"{self.name} encounter",
                       self.x+dx, self.y+dy, self.z+z, runner=runner)


def homes(site, ranch=False):
    w, d = min(site.width*.69, 16), min(site.depth*.73, 12)
    if ranch:
        w = site.width*.58
    h = 3.0
    site.building("_house", w, d, h, 1.35, finish="timber")
    # Paired pitched surfaces preserve a recognizable roofline on cabins and farmhouses.
    for side in (-1, 1):
        site.part(f"roof_pitch_{side}", "Pitched roof", side*w/4, 0, h+.62,
                  w*.56, d+.75, .19, "dark_trim", rotation=(-side*17, 0))
    for side in (-1, 1):
        offset = side*w*.25
        span = (0, w/2-.18) if side>0 else (-w/2+.18, 0)
        site.partition(f"bedrooms_{side}", d*.04, span, offset, 1.3, 2.95)
        site.part(f"dresser_{side}", "Bedroom dresser", offset, d*.39, 0, 1.6, .54, .92, "timber")
        site.part(f"bed_{side}", "Mattress", offset+side*.9, d*.28, .39, .95, 1.9, .17, "linen")
        site.part(f"bed_base_{side}", "Bed frame", offset+side*.9, d*.28, 0, .95, 1.9, .39, "timber")
        site.part(f"bed_lamp_{side}", "Nightstand", offset-side*.85, d*.38,
                  0, .47, .47, .54, "timber")
    site.part("bedroom_divider", "Bedroom divider", 0, d*.27, 0, .22, d*.44, 2.95, site.wall)
    site.part("hearth", "Fireplace", -w*.39, -d*.24, 0, .8, 1.24, 1.27, "concrete")
    site.part("table", "Kitchen table", w*.27, -d*.22, 0, 1.4, .8, .78, "timber")
    site.storage("pantry", "Household provisions", -w*.27, d*.23, food_amount=1, water_amount=2)
    if ranch:
        dx = site.width*.415
        site.building("_tool_shed", 3.15, 4.3, 2.75, 1.4, dx=dx, dy=site.depth*.24)
        site.storage("tools", "Farm tools and timber", dx, site.depth*.24+.93, timber_amount=10)
        for row in range(3):
            site.part(f"field_{row}", "Fallow planting bed", site.width*.414, -site.depth*.26+row*.74,
                      0, 2.4, .43, .15, "soil", collision=False)
    else:
        site.walker("resident", w*.28, d*.23)


def retail(site, use_windows):
    w, d = min(site.width*.78, 28), min(site.depth*.73, 16)
    front = site.building("_market", w, d, 3.65, 2.6, finish="petrol")
    site.label(front)
    if use_windows:
        for side in (-1, 1):
            site.window(f"{site.key}_display_{side}", site.x+side*w*.24, front, site.z)
    for i in range(4):
        dx = (i-1.5)*w*.15
        site.part(f"shelf_{i}_base", "Retail gondola base", dx, -d*.05, 0, .63, d*.39, .25, "dark_trim")
        for tier in range(3):
            site.part(f"shelf_{i}_{tier}", "Retail shelving", dx, -d*.05, .45+tier*.43,
                      .64, d*.39, .065, "petrol")
        site.part(f"shelf_{i}_back", "Shelf back", dx, -d*.05, .25, .11, d*.39, 1.15, "dark_trim")
    site.part("checkout", "Checkout counter", w*.33, -d*.31, 0, 2.1, .75, 1.0, "timber")
    site.partition("stockroom", d*.24, (-w/2+.12,w/2-.12), 0, 1.55, 3.5,
                   label="Stockroom door", finish="petrol")
    for i in range(2):
        site.furniture(f"{site.key}_stock_shelf_{i}", "Weathered stockroom shelf", "shelf",
                       site.x-w*.27+i*1.12, site.y+d*.42, site.z)
    site.storage("inventory", "Stockroom supplies", w*.25, d*.36, food_amount=3, water_amount=3)
    site.walker("shop_walker", -w*.33, d*.13)


def civic(site, medical=False, classroom=False):
    w, d = min(site.width*.77, 40), min(site.depth*.66, 24)
    front = site.building("_main", w, d, 4.25, 3.0, finish="petrol")
    site.label(front)
    for side in (-1, 1):
        dx = side*w*.246
        site.part(f"pier_{side}", "Entrance pier", side*w*.18, front-site.y-1.45,
                  0, .55, .55, 3.45, "classical_civic")
        span = (0, w/2-.15) if side>0 else (-w/2+.15, 0)
        site.partition(f"wing_{side}", d*.06, span, dx, 1.55, 3.7,
                       label="Treatment room door" if medical else "Office door", finish="petrol")
        if medical:
            site.bed(f"{site.key}_bed_{side}", "Treatment bed", site.x+dx, site.y+d*.30, site.z)
        elif classroom:
            for row in range(2):
                for column in range(3):
                    site.furniture(f"{site.key}_student_{side}_{row}_{column}", "Student desk", "desk",
                                   site.x+dx+(column-1)*1.35, site.y+d*(.21+row*.12), site.z)
        else:
            site.part(f"worktables_{side}", "Work tables", dx, d*.22, 0, 3.5, 1.1, .84, "timber")
        site.part(f"cabinets_{side}", "Wall cabinets", dx, d*.43, 0, 3.3, .58, 1.73, "dark_trim")
        site.part(f"waiting_{side}", "Waiting area bench", side*w*.37, -d*.24,
                  0, 2.1, .58, .46, "petrol")
    site.part("wing_divider", "Interior masonry", 0, d*.28, 0, .22, d*.44, 4.16, site.wall)
    site.part("reception", "Reception station", -2.5, -d*.13, 0, 2.7, .88, 1.02, "timber")
    site.part("porch", "Entry canopy", 0, front-site.y-.84, 3.42, w*.49, 1.95, .27, "classical_civic")
    site.storage("wing_case", "Emergency supplies" if medical else "Records desk",
                 w*.3, d*.37, cloth_amount=4 if medical else 1,
                 dressing_schematic_amount=int(medical), scavenger_notes_amount=int(not medical))
    site.walker("room_walker", -w*.24, d*.29)


def industrial(site, kind):
    w, d = min(site.width*.76, 42), min(site.depth*.71, 25)
    front = site.building("_works", w, d, 5.35, 4.5 if kind!="terminal" else 3.2, finish="petrol")
    # The loading opening stays unobstructed on the approach.
    if kind=="terminal":
        site.label(front-.14)
    else:
        site.sign(site.key+"_loading_sign", site.name.upper(), site.x+2.5, front-.27, site.z+4.45, size=42)
    for side in (-1, 1):
        for index in range(3):
            dy = -d*.26+index*d*.16
            site.part(f"rack_{side}_{index}", "Industrial storage", side*w*.37, dy,
                      0, 1.58, 1.08, 1.4, "petrol")
            site.part(f"load_{side}_{index}", "Crated cargo", side*w*.37, dy,
                      1.42, 1.37, .84, .66, "timber")
        site.part(f"apron_line_{side}", "Loading lane stripe", side*2.9, front-site.y-2.5,
                  .014, .12, 4.65, .023, "traffic", collision=False)
    site.partition("service_room", d*.19, (-w/2+.12,w/2-.12), 0, 1.55, 5.24,
                   label="Service room door", finish="petrol")
    if kind=="power":
        for i in range(3):
            dx = (i-1)*w*.24
            site.part(f"equipment_{i}", "Generator bank", dx, d*.35, 0, 2.25, 2, 1.65, "dark_trim")
            site.part(f"stack_{i}", "Exhaust stack", dx, d*.34, 5.6, .86, .86, 4+i*.6, "concrete")
    elif kind=="terminal":
        site.part("counter", "Check-in counter", -w*.22, -d*.13, 0, 8.6, .82, 1.12, "petrol")
        site.part("observation", "Terminal observation room", w*.29, d*.19, 5.60,
                  5.4, 4.0, 2.7, "modern")
    else:
        site.part("bench", "Maintenance workbench", -w*.24, d*.33, 0, 3.65, .89, .96, "timber")
    site.storage("recovery", "Maintenance locker", w*.27, d*.34, timber_amount=9, stone_amount=4)
    site.walker("yard_walker", -w*.23, .3, runner=kind=="power")


def offices(site, count):
    w, d, storey = site.width*.63, site.depth*.66, 3.15
    front = site.building("_tower", w, d, count*storey, 3.0, finish="petrol")
    site.label(front)
    shaft = 5.1
    shaft_x = w/2-shaft/2-.20
    stair_run = min(d*.67, 14)
    half = stair_run/2
    landing = d/2-half-.12
    for i in range(count):
        z = i*storey
        if i:
            site.part(f"floor_{i}", "Walkable upper floor", -shaft/2-.07, 0, z-.23,
                      w-shaft-.4, d-.24, .23, "concrete")
            for side in (-1, 1):
                site.part(f"landing_{i}_{side}", "Stair landing", shaft_x, side*(half+landing/2), z-.23,
                          shaft+.02, landing, .23, "concrete")
        if i+1<count:
            direction = 1 if i%2==0 else -1
            site.ramp(f"{site.key}_stairs_{i}", "Stairwell ramp", site.x+shaft_x-direction*1.22,
                      site.y-direction*half, site.y+direction*half, site.z+z, site.z+z+storey,
                      2.36, site.palette["concrete"])
        for side in (-1, 1):
            site.part(f"rail_{i}_{side}", "Stairwell guard", shaft_x+side*(shaft/2+.01),
                      0, z, .10, stair_run-.1, .95, "dark_trim")
            site.part(f"glass_band_{i}_{side}", "Window band", side*w*.27, -d/2-.035, z+1.03,
                      w*.26, .08, 1.36, "glazing", collision=False)
        site.partition(f"suite_{i}", d*.12, (-w/2+.12,w/2-shaft-.3), -w*.24, 1.5,
                       2.9, z, label="Office suite door", finish="petrol")
        site.part(f"desk_{i}", "Shelved work station", -w*.28, d*.33, z, 3.3, 1.1, 1.15, "timber")
        if i in (0,count-1):
            site.storage(f"case_{i}", "Floor supply locker", -w*.13, d*.33, z, water_amount=2, cloth_amount=2)
    site.walker("lobby_walker", -w*.28, -d*.15)
    site.walker("upper_walker", -w*.3, -d*.16, (count-1)*storey)


def place_site(box, door, ramp, cache, encounter, sign, window, bed, furniture, shell, pair,
               key, descriptor, x, y, z, w, d, palette):
    site = Site(box, door, ramp, cache, encounter, sign, window, bed, furniture, shell, pair, palette,
                key, descriptor["name"], x, y, z, w, d, descriptor["architecture"])
    kind, ident = descriptor["kind"], descriptor["id"]
    if kind in {"village", "suburbs", "ranch"}:
        homes(site, ranch=kind=="ranch")
    elif kind=="shops":
        retail(site, use_windows=ident not in {"car-lot","dock-strip"})
    elif kind in {"clinic","hospital","civic"}:
        civic(site, medical=kind in {"clinic","hospital"}, classroom=ident in {"school","university"})
        if ident=="stadium":
            site.part("service_pitch", "Practice court", 0, -d*.402, .015, w*.76, d*.17, .07,
                      "soil", collision=False)
    elif kind in {"industry","airport"}:
        industrial(site, "terminal" if kind=="airport" else "power" if ident.startswith("power-") else "works")
    elif kind=="highrise":
        offices(site, 6 if ident=="city-towers" else 4)
    else:
        raise ValueError(f"No interior layout for {ident} ({kind})")
