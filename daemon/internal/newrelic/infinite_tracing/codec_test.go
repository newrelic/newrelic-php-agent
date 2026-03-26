package infinite_tracing

import (
	"testing"

	"google.golang.org/grpc/encoding"

	v1 "github.com/newrelic/newrelic-php-agent/daemon/internal/newrelic/infinite_tracing/com_newrelic_trace_v1"
)

func TestProtoCodecRegistration(t *testing.T) {
	// Verify that our init() function properly registered the proto codec
	codec := encoding.GetCodec("proto")
	if codec == nil {
		t.Fatal("Proto codec is nil - registration failed")
	}
	if codec.Name() != "proto" {
		t.Errorf("Expected codec name 'proto', got %q", codec.Name())
	}
}

func TestCustomCodecWithProperRegistration(t *testing.T) {
	// Test our custom codec with properly registered proto codec
	protoCodec := encoding.GetCodec("proto")
	if protoCodec == nil {
		t.Fatal("Proto codec not registered - cannot test custom codec")
	}

	c := &codec{protoCodec}

	// Test marshaling of encodedSpanBatch (should use our custom logic)
	batch := encodedSpanBatch("test span data")
	data, err := c.Marshal(batch)
	if err != nil {
		t.Fatalf("Failed to marshal encoded span batch: %v", err)
	}
	if string(data) != "test span data" {
		t.Errorf("Expected 'test span data', got %q", string(data))
	}

	// Test marshaling/unmarshaling of RecordStatus (should use underlying proto codec)
	status := &v1.RecordStatus{MessagesSeen: 42}

	// Marshal using underlying proto codec
	statusData, err := c.Marshal(status)
	if err != nil {
		t.Fatalf("Failed to marshal RecordStatus: %v", err)
	}

	// Unmarshal it back
	result := &v1.RecordStatus{}
	err = c.Unmarshal(statusData, result)
	if err != nil {
		t.Fatalf("Failed to unmarshal RecordStatus: %v", err)
	}

	if result.MessagesSeen != 42 {
		t.Errorf("Expected MessagesSeen=42, got %d", result.MessagesSeen)
	}
}

func TestCodecName(t *testing.T) {
	protoCodec := encoding.GetCodec("proto")
	if protoCodec == nil {
		t.Fatal("Proto codec not registered")
	}

	c := &codec{protoCodec}
	if c.Name() != "proto" {
		t.Errorf("Expected Name()='proto', got %q", c.Name())
	}

	// Test with nil codec (should still return "proto" as fallback)
	nilCodec := &codec{nil}
	if nilCodec.Name() != "proto" {
		t.Errorf("Expected Name()='proto' for nil codec, got %q", nilCodec.Name())
	}
}