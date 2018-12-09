#
# This code verifies that the time broadcasts are happening
#


class Timetest

def initialize
	@startTime = Time.new().to_i
	@endTime = @startTime + 121
	@btime = 0
end

def name
	return "Timetest"
end

def run(stateHash)
	if Time.new().to_i > @endTime
		print "TimeTest FAILED: no update within 2 minutes\n"
		return :test_done
	end

	broadcast = stateHash['$broadcast']
	return :test_running if broadcast.nil?

	iottime = broadcast['IOTtime']
	return :test_running if iottime.nil?

	print "IOTtime is #{iottime}\n" if $debug

	t = iottime.to_i
	if @btime == 0
		@btime = t
		return :test_running
	elsif @btime == t
		return :test_running
	elsif t - @btime == 60
		passed("TimeTest")
		print "\n"
		return :test_done
	else
		failed("TimeTest")
		printf ": update by %d rather than 60\n", t - @btime
		return :test_done
	end
end

end #end of class
