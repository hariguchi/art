#
# usage: dumpNode <address>
#
define dumpNode
  if ($argc > 0)
    printf "level: %d\n", ((size_t*)$arg0)[-1]
    printf "%d elements\n", ((size_t*)$arg0)[0]
    x/16bx &(((size_t*)$arg0)[-2])
    x/32gx $arg0
  end
end

#
# usage: dumpNodePc <address>
#
define dumpNodePc
  if ($argc > 0)
    printf "level: %d\n", ((size_t*)$arg0)[-1]
    printf "%d routes\n", ((subtable)$arg0)[0].nRoutes
    printf "%d subtables\n", ((subtable)$arg0)[0].nSubtables
    x/16bx &(((size_t*)$arg0)[-2])
    x/32gx $arg0
  end
end
