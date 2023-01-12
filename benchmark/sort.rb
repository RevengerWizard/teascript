a = []
1000000.times {|i| a << Random.rand(-10000...10000)}

start = Time.now

a.sort

puts "elapsed: " + (Time.now - start).to_s