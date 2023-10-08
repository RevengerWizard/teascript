# Map

#### Properties

-   `Map.len`: the number of key-value pairs in the map.
-   `Map.keys`: the keys in the map as a list.
-   `Map.items`: the items in the map as a list.

#### Example

```tea
var m = { a = 1, b = 2, c = 3 }
print(m.len)   // 3
print(m.keys)   // ["a", "b", "c"]
print(m.items)   // [1, 2, 3]
```

---

## Map.clear

The `Map.clear` method removes all key-value pairs from the map.

#### Syntax

```tea
Map.clear()
```

#### Parameters

None.

#### Return Value

None.

#### Example

```tea
var m = {a = 1, b = 2, c = 3}
m.clear()
print(m)  // {}
```

---

## Map.contains

The `Map.contains` method checks wether the map contains a certain key.

#### Syntax

```tea
Map.contains(key)
```

#### Parameters

-   `key`: The key to check for in the map.

#### Return Value

`true` if the map contains the specified key, and `false` otherwise.

#### Example

```tea
var m = {a = 1, b = 2, c = 3}
print(m.contains("a"))  // true
print(m.contains("d"))  // false
```

---

## Map.delete

The `Map.delete` method removes the key-value pair with the specified key from the map.

#### Syntax

```tea
Map.delete(key)
```

#### Parameters

-   `key`: The key of the key-value pair to delete from the map.

#### Return Value

None.

#### Example

```tea
var m = {a = 1, b = 2, c = 3}
m.delete("b")
print(m)  // {a = 1, c = 3}
print(m.contains("b"))  // false
```

---

## Map.copy

The `Map.copy` method returns a new map that is a shallow copy of the original map.

#### Syntax

```tea
Map.copy()
```

#### Parameters

None.

#### Return Value

A new map that is a shallow copy of the original map.

#### Example

```tea
var m = {a = 1, b = 2, c = 3}
var n = m.copy()
print(m == n)  // true
print(m is n)  // false
```