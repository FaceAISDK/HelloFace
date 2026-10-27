import Foundation
import CoreGraphics
import ImageIO
import UniformTypeIdentifiers
import HiFace
import XCTest
@testable import HiFaceDemo

/// Run inside the real Demo App against the binary SDK; exercise model resources and HF1 storage without a camera.
/// 在真实 Demo App 内针对二进制 SDK 运行，无需相机即可验证模型资源与 HF1 存储。
final class ModelResourceTests: XCTestCase {
    private func requireDeviceRuntime() throws {
        #if targetEnvironment(simulator)
        throw XCTSkip("The simulator SDK is compile/link-only; native runtime acceptance requires a physical iOS device")
        #endif
    }

    @MainActor
    func testSimulatorStubReportsUnavailableWithoutPretendingToRunModels() async throws {
        #if targetEnvironment(simulator)
        let sdk = HiFaceSDK()
        defer { sdk.close() }
        switch try await extractBlankImageFeature(sdk) {
        case .failure(let error):
            XCTAssertEqual(error.code, .modelLoadFailed)
            XCTAssertTrue(error.message.contains("iOS Simulator"))
        case .success:
            XCTFail("The simulator stub must not claim successful native inference")
        }
        #else
        throw XCTSkip("Simulator-only contract; device runtime is covered by the native tests")
        #endif
    }

    @MainActor
    func testImageSearchPreservesOriginalResolutionAndOddPixelCrop() async throws {
        for (width, height) in [(4032, 3024), (3024, 4032), (4096, 1024), (2049, 1025), (100, 100)] {
            try autoreleasepool {
                let encoded = try photoData(width: width, height: height)
                let temporary = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".png")
                try encoded.write(to: temporary)
                defer { try? FileManager.default.removeItem(at: temporary) }
                let decoded = try StaticPhotoDecoder.decode(fileURL: temporary,
                    maximumPixelSize: FacePhotoPicker.imageSearchMaximumPixelSize).get()
                XCTAssertEqual(decoded.width, width & ~1)
                XCTAssertEqual(decoded.height, height & ~1)
                XCTAssertEqual(decoded.rgba.count, (width & ~1) * (height & ~1) * 4)
            }
        }
    }

    @MainActor
    func testImageSearchCapAndExifRotationKeepComparisonDefault() async throws {
        for (width, height, orientation, expectedWidth, expectedHeight) in [
            (6000, 3000, 1, 4096, 2048), (3000, 6000, 1, 2048, 4096),
            (2049, 1025, 6, 1024, 2048)
        ] {
            try autoreleasepool {
                let encoded = try photoData(width: width, height: height, orientation: orientation)
                let temporary = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString + ".png")
                try encoded.write(to: temporary)
                defer { try? FileManager.default.removeItem(at: temporary) }
                let decoded = try StaticPhotoDecoder.decode(fileURL: temporary,
                    maximumPixelSize: FacePhotoPicker.imageSearchMaximumPixelSize).get()
                XCTAssertEqual(decoded.width, expectedWidth)
                XCTAssertEqual(decoded.height, expectedHeight)
            }
        }
        let comparison = try StaticPhotoDecoder.decode(data: photoData(width: 3000, height: 2000)).get()
        XCTAssertEqual(comparison.width, 1080)
        XCTAssertEqual(comparison.height, 720)
    }

    private func photoData(width: Int, height: Int, orientation: Int = 1) throws -> Data {
        let context = try XCTUnwrap(CGContext(data: nil, width: width, height: height, bitsPerComponent: 8,
            bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
            bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue))
        context.setFillColor(CGColor(red: 0.2, green: 0.4, blue: 0.6, alpha: 1))
        context.fill(CGRect(x: 0, y: 0, width: width, height: height))
        let image = try XCTUnwrap(context.makeImage())
        let data = NSMutableData()
        let destination = try XCTUnwrap(CGImageDestinationCreateWithData(data, UTType.png.identifier as CFString, 1, nil))
        CGImageDestinationAddImage(destination, image, [kCGImagePropertyOrientation: orientation] as CFDictionary)
        XCTAssertTrue(CGImageDestinationFinalize(destination))
        return data as Data
    }

    @MainActor
    func testImageFacesReturnEmptyOnBlankPhoto() async throws {
        try requireDeviceRuntime()
        let sdk = HiFaceSDK()
        defer { sdk.close() }
        let image = try HiFaceRGBAImage(rgba: Data(repeating: 128, count: 332 * 332 * 4), width: 332, height: 332)
        let faces = try await sdk.captureFaces(image).get()
        XCTAssertTrue(faces.isEmpty, "An empty detection is successful; it must not manufacture a capture")
    }

    @MainActor
    func testQ7DatabasePreservesHF1ScoresAndReopens() async throws {
        try requireDeviceRuntime()
        let resource = try XCTUnwrap(Bundle.main.url(forResource: "features_10000", withExtension: "json"))
        let manifest = try XCTUnwrap(JSONSerialization.jsonObject(with: Data(contentsOf: resource)) as? [String: Any])
        let records = try XCTUnwrap(manifest["entries"] as? [[String: Any]])
        let first = try XCTUnwrap(records[0]["feature"] as? String)
        let second = try XCTUnwrap(records[1]["feature"] as? String)
        let group = "q7-regression-" + UUID().uuidString
        let sdk = HiFaceSDK()
        defer { sdk.close() }
        let score = try await sdk.compare(feature: first, with: second).get()
        let configuration = try FaceSearchConfiguration(group: group, topK: 2, threshold: 0)
        let database = try HiFaceSDK.openSearchDatabase()
        do {
            try await database.saveFeature(faceID: group + "-a", feature: first, group: group)
            try await database.saveFeatures([
                try FaceSearchFeatureEntry(faceID: group + "-b", feature: second, group: group)
            ])
            let before = try await database.search(feature: first, configuration: configuration)
            XCTAssertEqual(before.count, 2)
            XCTAssertEqual(before.first?.score, 1)
            XCTAssertEqual(before.last?.score, score)
            let firstEntry = try await database.entry(faceID: group + "-a")
            XCTAssertEqual(firstEntry.feature, first)
            XCTAssertEqual(firstEntry.featureSpaceID, FaceFeatureFormat.featureSpaceID)
            try await database.updateMetadata(faceID: group + "-a", tag: "metadata-only", updateTimeMilliseconds: 123)
            let updated = try await database.entry(faceID: group + "-a")
            XCTAssertEqual(updated.feature, first)
            XCTAssertEqual(updated.group, group)
            XCTAssertEqual(updated.tag, "metadata-only")
            let metadata = try await database.recentEntries(group: group)
            XCTAssertEqual(metadata.count, 2)
            let exported = try await database.featureEntries(group: group)
            XCTAssertEqual(exported.map(\.feature), [first, second])
            do {
                _ = try await database.featureEntries(offset: Int(Int32.max) + 1)
                XCTFail("An overflowing page offset must be rejected")
            } catch let error as HiFaceError {
                XCTAssertEqual(error.code, .invalidArgument)
            }
            try await database.updateMetadata(faceID: group + "-a", tag: "", updateTimeMilliseconds: firstEntry.updateTimeMilliseconds)
            await database.close()

            let reopened = try HiFaceSDK.openSearchDatabase()
            do {
                let after = try await reopened.search(feature: first, configuration: configuration)
                XCTAssertEqual(after, before)
                let restored = try await reopened.entry(faceID: group + "-a")
                XCTAssertEqual(restored.feature, firstEntry.feature)
                _ = try await reopened.delete(faceID: group + "-a")
                _ = try await reopened.delete(faceID: group + "-b")
                await reopened.close()
            } catch {
                _ = try? await reopened.delete(faceID: group + "-a")
                _ = try? await reopened.delete(faceID: group + "-b")
                await reopened.close()
                throw error
            }
        } catch {
            _ = try? await database.delete(faceID: group + "-a")
            _ = try? await database.delete(faceID: group + "-b")
            await database.close()
            throw error
        }
    }

    @MainActor
    func testDemoLoadsFrameworkModelsWithoutAnAppModelDirectory() async throws {
        try requireDeviceRuntime()
        let appModels = try XCTUnwrap(Bundle.main.resourceURL)
            .appendingPathComponent("HiFaceModels", isDirectory: true)
        XCTAssertFalse(FileManager.default.fileExists(atPath: appModels.path),
                       "This regression must exercise SDK-owned resources, without an App model copy")
        let sdk = HiFaceSDK()
        defer { sdk.close() }

        switch try await extractBlankImageFeature(sdk) {
        case .failure(let error):
            XCTAssertEqual(error.code, .imageNoFace,
                           "Native detector and feature models must load before rejecting a blank image: \(error.message)")
        case .success:
            XCTFail("A blank image must not produce a face feature")
        }
    }

    @MainActor
    func testAnExplicitMissingModelDirectoryReportsModelLoadFailure() async throws {
        try requireDeviceRuntime()
        let missingModels = try XCTUnwrap(Bundle.main.resourceURL)
            .appendingPathComponent("MissingHiFaceModels", isDirectory: true)
        let sdk = HiFaceSDK(configuration: HiFaceConfiguration(modelDirectoryURL: missingModels))
        defer { sdk.close() }

        switch try await extractBlankImageFeature(sdk) {
        case .failure(let error):
            XCTAssertEqual(error.code, .modelLoadFailed)
            XCTAssertTrue(error.message.contains("kcehc.bphfm"))
        case .success:
            XCTFail("An explicit missing model directory must not silently use another resource location")
        }
    }

    @MainActor
    private func extractBlankImageFeature(_ sdk: HiFaceSDK) async throws -> Result<String, HiFaceError> {
        let image = try HiFaceRGBAImage(rgba: Data(repeating: 0, count: 64 * 64 * 4), width: 64, height: 64)
        for _ in 0..<250 {
            let result = await sdk.extractFeature(from: image)
            if case .failure(let error) = result, error.code == .notReady {
                try await Task.sleep(nanoseconds: 20_000_000)
                continue
            }
            return result
        }
        XCTFail("Native SDK initialization did not complete within five seconds")
        return .failure(HiFaceError(code: .notReady, message: "Initialization timeout"))
    }
}
