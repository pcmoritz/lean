import leanpy

context = leanpy.initialize("/home/pcmoritz/lean/library")

l = leanpy.LevelList([leanpy.level.zero, leanpy.level.zero])

print(l)

leanpy.expr.const(leanpy.name(), l)

context.get(leanpy.name("or"))
