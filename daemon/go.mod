module github.com/newrelic/newrelic-php-agent/daemon

go 1.24.0
toolchain go1.24.4

require (
	github.com/golang/protobuf v1.5.3
	github.com/google/flatbuffers v23.5.26+incompatible
	golang.org/x/net v0.38.0
	google.golang.org/grpc v1.61.0
	google.golang.org/protobuf v1.33.0
)

require (
	golang.org/x/sys v0.31.0 // indirect
	golang.org/x/text v0.23.0 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20231106174013-bbf56f31fb17 // indirect
)
