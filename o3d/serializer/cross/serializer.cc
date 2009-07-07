/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the definitions of class Serializer.

#include <vector>

#include "serializer/cross/serializer.h"
#include "serializer/cross/serializer_binary.h"

#include "core/cross/buffer.h"
#include "core/cross/curve.h"
#include "core/cross/iclass_manager.h"
#include "core/cross/pack.h"
#include "core/cross/param_array.h"
#include "core/cross/primitive.h"
#include "core/cross/shape.h"
#include "core/cross/skin.h"
#include "core/cross/texture.h"
#include "core/cross/transform.h"
#include "import/cross/destination_buffer.h"
#include "import/cross/iarchive_generator.h"
#include "import/cross/memory_buffer.h"
#include "import/cross/memory_stream.h"
#include "serializer/cross/version.h"
#include "utils/cross/structured_writer.h"

using std::string;

namespace o3d {

const char* Serializer::ROOT_PREFIX = "o3d_rootObject_";

void Serialize(StructuredWriter* writer, ObjectBase* value) {
  if (value == NULL) {
    writer->WriteNull();
  } else {
    writer->BeginCompacting();
    writer->OpenObject();
    writer->WritePropertyName("ref");
    writer->WriteInt(GetObjectId(value));
    writer->CloseObject();
    writer->EndCompacting();
  }
}

void Serialize(StructuredWriter* writer, float value) {
  writer->WriteFloat(value);
}

void Serialize(StructuredWriter* writer, const Float2& value) {
  writer->BeginCompacting();
  writer->OpenArray();
  for (int i = 0; i < 2; ++i)
    writer->WriteFloat(value[i]);
  writer->CloseArray();
  writer->EndCompacting();
}

void Serialize(StructuredWriter* writer, const Float3& value) {
  writer->BeginCompacting();
  writer->OpenArray();
  for (int i = 0; i < 3; ++i)
    writer->WriteFloat(value[i]);
  writer->CloseArray();
  writer->EndCompacting();
}

void Serialize(StructuredWriter* writer, const Float4& value) {
  writer->BeginCompacting();
  writer->OpenArray();
  for (int i = 0; i < 4; ++i)
    writer->WriteFloat(value[i]);
  writer->CloseArray();
  writer->EndCompacting();
}

void Serialize(StructuredWriter* writer, int value) {
  writer->WriteInt(value);
}

void Serialize(StructuredWriter* writer, unsigned int value) {
  writer->WriteUnsignedInt(value);
}

void Serialize(StructuredWriter* writer, bool value) {
  writer->WriteBool(value);
}

void Serialize(StructuredWriter* writer, const String& value) {
  writer->WriteString(value);
}

void Serialize(StructuredWriter* writer, const Matrix4& value) {
  writer->BeginCompacting();
  writer->OpenArray();
  for (int i = 0; i < 4; ++i) {
    writer->OpenArray();
    for (int j = 0; j < 4; ++j)
      writer->WriteFloat(value[i][j]);
    writer->CloseArray();
  }
  writer->CloseArray();
  writer->EndCompacting();
}

void Serialize(StructuredWriter* writer, const BoundingBox& value) {
  writer->BeginCompacting();
  writer->OpenArray();

  if (value.valid()) {
    writer->OpenArray();
    for (int i = 0; i < 3; ++i)
      writer->WriteFloat(value.min_extent()[i]);
    writer->CloseArray();
    writer->OpenArray();
    for (int i = 0; i < 3; ++i)
      writer->WriteFloat(value.max_extent()[i]);
    writer->CloseArray();
  }

  writer->CloseArray();
  writer->EndCompacting();
}

void Serialize(StructuredWriter* writer, const Stream& stream) {
  writer->OpenObject();

  writer->WritePropertyName("field");
  Serialize(writer, GetObjectId(&stream.field()));

  writer->WritePropertyName("startIndex");
  Serialize(writer, stream.start_index());

  writer->WritePropertyName("semantic");
  Serialize(writer, stream.semantic());

  writer->WritePropertyName("semanticIndex");
  Serialize(writer, stream.semantic_index());

  writer->CloseObject();
}

void BinaryArchiveManager::WriteObjectBinary(ObjectBase* object,
                                             const string& file_name,
                                             const uint8* data,
                                             size_t numBytes) {
  FileContent& content = file_map_[file_name];
  ObjectBinaryRangeMap::iterator it = object_binary_range_map_.find(object);
  BinaryRange range;
  if (it != object_binary_range_map_.end()) {
    // It is okay to call WriteObjectBinary multiple times for a single
    // object but they must not be interleaved with those for other objects.
    range = it->second;
    DCHECK(range.end_offset_ == content.size());
  } else {
    range.begin_offset_ = content.size();
  }
  content.insert(content.end(), data, data + numBytes);
  range.end_offset_ = content.size();
  object_binary_range_map_[object] = range;
}

BinaryRange BinaryArchiveManager::GetObjectRange(ObjectBase* object) {
  return object_binary_range_map_[object];
}

void BinaryArchiveManager::WriteArchive(IArchiveGenerator* archive_generator) {
  for (FileMap::const_iterator it = file_map_.begin();
       it != file_map_.end();
       ++it) {
    const string& file_name = it->first;
    const FileContent& content = it->second;
    archive_generator->AddFile(file_name,
                               content.size());
    MemoryReadStream stream(&content[0],
                            content.size());
    archive_generator->AddFileBytes(&stream, content.size());
  }
}

namespace {
bool ParamIsSerialized(Param* param) {
  return param->output_connections().size() != 0 ||
      (!param->dynamic() && !param->read_only());
}

class PropertiesVisitor : public VisitorBase<PropertiesVisitor> {
 public:
  explicit PropertiesVisitor(StructuredWriter* writer)
      : writer_(writer) {
    Enable<Curve>(&PropertiesVisitor::Visit);
    Enable<Element>(&PropertiesVisitor::Visit);
    Enable<NamedObject>(&PropertiesVisitor::Visit);
    Enable<Pack>(&PropertiesVisitor::Visit);
    Enable<Primitive>(&PropertiesVisitor::Visit);
    Enable<Shape>(&PropertiesVisitor::Visit);
    Enable<Skin>(&PropertiesVisitor::Visit);
    Enable<Transform>(&PropertiesVisitor::Visit);
  }

 private:
  void Visit(Curve* curve) {
    Visit(static_cast<NamedObject*>(curve));

    writer_->WritePropertyName("preInfinity");
    Serialize(writer_, curve->pre_infinity());

    writer_->WritePropertyName("postInfinity");
    Serialize(writer_, curve->post_infinity());

    writer_->WritePropertyName("useCache");
    Serialize(writer_, curve->use_cache());

    writer_->WritePropertyName("sampleRate");
    Serialize(writer_, curve->sample_rate());
  }

  void Visit(Element* element) {
    Visit(static_cast<NamedObject*>(element));

    writer_->WritePropertyName("owner");
    Serialize(writer_, element->owner());
  }

  void Visit(NamedObject* object) {
    Visit(static_cast<ObjectBase*>(object));

    if (object->name().length() > 0) {
      writer_->WritePropertyName("name");
      Serialize(writer_, object->name());
    }
  }

  void Visit(ObjectBase* object) {
  }

  void Visit(Pack* pack) {
    Visit(static_cast<NamedObject*>(pack));

    writer_->WritePropertyName("root");
    Serialize(writer_, pack->root());
  }

  void Visit(Primitive* primitive) {
    Visit(static_cast<Element*>(primitive));

    writer_->WritePropertyName("numberVertices");
    Serialize(writer_, primitive->number_vertices());

    writer_->WritePropertyName("numberPrimitives");
    Serialize(writer_, primitive->number_primitives());

    writer_->WritePropertyName("primitiveType");
    Serialize(writer_, primitive->primitive_type());

    writer_->WritePropertyName("indexBuffer");
    Serialize(writer_, primitive->index_buffer());

    writer_->WritePropertyName("startIndex");
    Serialize(writer_, primitive->start_index());
  }

  void Visit(Shape* shape) {
    Visit(static_cast<ParamObject*>(shape));

    writer_->WritePropertyName("elements");
    writer_->BeginCompacting();
    writer_->OpenArray();
    const ElementRefArray& elements = shape->GetElementRefs();
    for (int i = 0; i != elements.size(); ++i) {
      Serialize(writer_, elements[i].Get());
    }
    writer_->CloseArray();
    writer_->EndCompacting();
  }

  void Visit(Skin* skin) {
    Visit(static_cast<NamedObject*>(skin));

    writer_->WritePropertyName("inverseBindPoseMatrices");
    writer_->BeginCompacting();
    writer_->OpenArray();
    const Skin::MatrixArray& inverse_bind_pose_matrices =
        skin->inverse_bind_pose_matrices();
    for (int i = 0; i != inverse_bind_pose_matrices.size(); ++i) {
      const Matrix4& matrix = inverse_bind_pose_matrices[i];
      Serialize(writer_, matrix);
    }
    writer_->CloseArray();
    writer_->EndCompacting();
  }

  void Visit(Transform* transform) {
    Visit(static_cast<ParamObject*>(transform));

    writer_->WritePropertyName("shapes");
    writer_->BeginCompacting();
    writer_->OpenArray();
    const ShapeRefArray& shape_array = transform->GetShapeRefs();
    for (int i = 0; i != shape_array.size(); ++i) {
      Serialize(writer_, shape_array[i]);
    }
    writer_->CloseArray();
    writer_->EndCompacting();

    writer_->WritePropertyName("parent");
    Serialize(writer_, transform->parent());
  }

  StructuredWriter* writer_;
};

class CustomVisitor : public VisitorBase<CustomVisitor> {
 public:
  explicit CustomVisitor(StructuredWriter* writer,
                         BinaryArchiveManager* binary_archive_manager)
      : writer_(writer),
        binary_archive_manager_(binary_archive_manager) {
    Enable<DestinationBuffer>(&CustomVisitor::Visit);
    Enable<Buffer>(&CustomVisitor::Visit);
    Enable<Curve>(&CustomVisitor::Visit);
    Enable<Primitive>(&CustomVisitor::Visit);
    Enable<Skin>(&CustomVisitor::Visit);
    Enable<SkinEval>(&CustomVisitor::Visit);
    Enable<StreamBank>(&CustomVisitor::Visit);
    Enable<Texture2D>(&CustomVisitor::Visit);
    Enable<TextureCUBE>(&CustomVisitor::Visit);
  }

 private:
  void Visit(DestinationBuffer* buffer) {
    // NOTE: We don't call Visit<VertexBuffer*> because we don't want to
    // serialize the contents of the Buffer. We only serialize its structure.
    Visit(static_cast<NamedObject*>(buffer));
    writer_->WritePropertyName("numElements");
    Serialize(writer_, buffer->num_elements());
    writer_->WritePropertyName("fields");
    writer_->OpenArray();
    const FieldRefArray& fields = buffer->fields();
    for (size_t ii = 0; ii < fields.size(); ++ii) {
      Field* field = fields[ii].Get();
      writer_->BeginCompacting();
      writer_->OpenObject();
      writer_->WritePropertyName("id");
      Serialize(writer_, field->id());
      writer_->WritePropertyName("type");
      Serialize(writer_, field->GetClassName());
      writer_->WritePropertyName("numComponents");
      Serialize(writer_, field->num_components());
      writer_->CloseObject();
      writer_->EndCompacting();
    }
    writer_->CloseArray();
  }

  void Visit(Buffer* buffer) {
    Visit(static_cast<NamedObject*>(buffer));

    writer_->WritePropertyName("fields");
    writer_->OpenArray();
    const FieldRefArray& fields = buffer->fields();
    for (size_t ii = 0; ii < fields.size(); ++ii) {
      Field* field = fields[ii].Get();
      Serialize(writer_, field->id());
    }
    writer_->CloseArray();

    WriteObjectBinaryRange(buffer);
  }

  void Visit(Curve* curve) {
    Visit(static_cast<NamedObject*>(curve));
    WriteObjectBinaryRange(curve);
  }

  void Visit(ObjectBase* object) {
  }

  void Visit(Primitive* primitive) {
    Visit(static_cast<Element*>(primitive));

    if (primitive->index_buffer()) {
      writer_->WritePropertyName("indexBuffer");
      Serialize(writer_, GetObjectId(primitive->index_buffer()));
    }
  }

  void Visit(Skin* skin) {
    Visit(static_cast<NamedObject*>(skin));
    WriteObjectBinaryRange(skin);
  }

  void Visit(SkinEval* skin_eval) {
    Visit(static_cast<VertexSource*>(skin_eval));

    writer_->WritePropertyName("vertexStreams");
    writer_->OpenArray();
    const StreamParamVector& vertex_stream_params =
        skin_eval->vertex_stream_params();
    for (int i = 0; i != vertex_stream_params.size(); ++i) {
      const Stream& stream = vertex_stream_params[i]->stream();
      writer_->OpenObject();
      writer_->WritePropertyName("stream");
      Serialize(writer_, stream);

      if (vertex_stream_params[i]->input_connection() != NULL) {
        writer_->WritePropertyName("bind");
        Serialize(writer_,
                  GetObjectId(
                      vertex_stream_params[i]->input_connection()->owner()));
      }

      writer_->CloseObject();
    }
    writer_->CloseArray();
  }

  void Visit(StreamBank* stream_bank) {
    Visit(static_cast<NamedObject*>(stream_bank));

    writer_->WritePropertyName("vertexStreams");
    writer_->OpenArray();
    const StreamParamVector& vertex_stream_params =
        stream_bank->vertex_stream_params();
    for (int i = 0; i != vertex_stream_params.size(); ++i) {
      const Stream& stream = vertex_stream_params[i]->stream();
      writer_->OpenObject();
      writer_->WritePropertyName("stream");
      Serialize(writer_, stream);

      if (vertex_stream_params[i]->input_connection() != NULL) {
        writer_->WritePropertyName("bind");
        Serialize(writer_,
                  GetObjectId(
                      vertex_stream_params[i]->input_connection()->owner()));
      }

      writer_->CloseObject();
    }
    writer_->CloseArray();
  }

  void Visit(Texture2D* texture) {
    Visit(static_cast<Texture*>(texture));

    writer_->WritePropertyName("width");
    Serialize(writer_, texture->width());
    writer_->WritePropertyName("height");
    Serialize(writer_, texture->height());
    writer_->WritePropertyName("format");
    Serialize(writer_, texture->format());
    writer_->WritePropertyName("levels");
    Serialize(writer_, texture->levels());
    writer_->WritePropertyName("renderSurfacesEnabled");
    Serialize(writer_, texture->render_surfaces_enabled());
  }

  void Visit(TextureCUBE* texture) {
    Visit(static_cast<Texture*>(texture));

    writer_->WritePropertyName("edgeLength");
    Serialize(writer_, texture->edge_length());
    writer_->WritePropertyName("format");
    Serialize(writer_, texture->format());
    writer_->WritePropertyName("levels");
    Serialize(writer_, texture->levels());
    writer_->WritePropertyName("renderSurfacesEnabled");
    Serialize(writer_, texture->render_surfaces_enabled());
  }

  void WriteObjectBinaryRange(ObjectBase* object) {
    writer_->WritePropertyName("binaryRange");
    writer_->BeginCompacting();
    writer_->OpenArray();
    BinaryRange range = binary_archive_manager_->GetObjectRange(object);
    writer_->WriteUnsignedInt(static_cast<unsigned int>(range.begin_offset_));
    writer_->WriteUnsignedInt(static_cast<unsigned int>(range.end_offset_));
    writer_->CloseArray();
    writer_->EndCompacting();
  }

  StructuredWriter* writer_;
  BinaryArchiveManager* binary_archive_manager_;
};

class ParamVisitor : public VisitorBase<ParamVisitor> {
 public:
  explicit ParamVisitor(StructuredWriter* writer)
      : writer_(writer) {
    Enable<ParamObject>(&ParamVisitor::Visit);
    Enable<ParamArray>(&ParamVisitor::Visit);
    Enable<ParamBoolean>(&ParamVisitor::Visit);
    Enable<ParamBoundingBox>(&ParamVisitor::Visit);
    Enable<ParamFloat>(&ParamVisitor::Visit);
    Enable<ParamFloat2>(&ParamVisitor::Visit);
    Enable<ParamFloat3>(&ParamVisitor::Visit);
    Enable<ParamFloat4>(&ParamVisitor::Visit);
    Enable<ParamInteger>(&ParamVisitor::Visit);
    Enable<ParamMatrix4>(&ParamVisitor::Visit);
    Enable<ParamString>(&ParamVisitor::Visit);
    Enable<RefParamBase>(&ParamVisitor::Visit);
    Enable<Material>(&ParamVisitor::Visit);
  }

 private:
  void Visit(Material* object) {
    Visit(static_cast<ParamObject*>(object));
  }

  void Visit(ParamObject* object) {
    const NamedParamRefMap& params = object->params();

    int numWrittenParams = 0;
    for (NamedParamRefMap::const_iterator it = params.begin();
         it != params.end(); ++it) {
      String param_name = it->first;
      Param::Ref param = it->second;
      if (ParamIsSerialized(param.Get())) {
        ++numWrittenParams;
      }
    }

    if (numWrittenParams > 0) {
      writer_->WritePropertyName("params");
      writer_->OpenObject();
      for (NamedParamRefMap::const_iterator it = params.begin();
           it != params.end(); ++it) {
        String param_name = it->first;
        Param::Ref param = it->second;
        if (ParamIsSerialized(param.Get())) {
          writer_->WritePropertyName(param_name);
          Accept(param.Get());
        }
      }
      writer_->CloseObject();
    }
  }

  void Visit(ParamArray* param_array) {
    writer_->WritePropertyName("params");
    writer_->OpenArray();
    const ParamArray::ParamRefVector& params = param_array->params();
    for (unsigned i = 0; i < params.size(); ++i) {
      Param::Ref param = params[i];
      Accept(param.Get());
    }
    writer_->CloseArray();
  }

  template <typename ParamType>
  void Visit(ParamType* param) {
    writer_->BeginCompacting();
    writer_->OpenObject();

    if (param->owner() == NULL || param->owner()->IsAddedParam(param)) {
      writer_->WritePropertyName("class");
      writer_->WriteString(param->GetClassName());
    }

    if (param->output_connections().size() != 0) {
      writer_->WritePropertyName("id");
      writer_->WriteInt(param->id());
    }
    if (param->input_connection() != NULL) {
      writer_->WritePropertyName("bind");
      writer_->WriteInt(param->input_connection()->id());
    } else if (!param->dynamic()) {
      writer_->WritePropertyName("value");
      Serialize(writer_, param->value());
    }

    writer_->CloseObject();
    writer_->EndCompacting();
  }

  StructuredWriter* writer_;
};

class BinaryVisitor : public VisitorBase<BinaryVisitor> {
 public:
  explicit BinaryVisitor(BinaryArchiveManager* binary_archive_manager)
      : binary_archive_manager_(binary_archive_manager) {
    Enable<Curve>(&BinaryVisitor::Visit);
    Enable<IndexBuffer>(&BinaryVisitor::Visit);
    Enable<DestinationBuffer>(&BinaryVisitor::Visit);
    Enable<VertexBufferBase>(&BinaryVisitor::Visit);
    Enable<Skin>(&BinaryVisitor::Visit);
  }

 private:
  void Visit(ObjectBase* object) {
  }

  // TODO: Replace this when we have code to serialize to the
  // final binary format. This is just placeholder.
  void Visit(Curve* curve) {
    Visit(static_cast<NamedObject*>(curve));

    MemoryBuffer<uint8> serialized_data;
    SerializeCurve(*curve, &serialized_data);

    binary_archive_manager_->WriteObjectBinary(
        curve,
        "curve-keys.bin",
        serialized_data,
        serialized_data.GetLength());
  }

  void Visit(DestinationBuffer* buffer) {
    // Destination buffers should NOT have their contents serialized.
    Visit(static_cast<Buffer*>(buffer));
  }

  void Visit(IndexBuffer* buffer) {
    Visit(static_cast<Buffer*>(buffer));

    MemoryBuffer<uint8> serialized_data;
    SerializeBuffer(*buffer, &serialized_data);

    binary_archive_manager_->WriteObjectBinary(
        buffer,
        "index-buffers.bin",
        serialized_data,
        serialized_data.GetLength());
  }

  void Visit(VertexBufferBase* buffer) {
    Visit(static_cast<Buffer*>(buffer));

    MemoryBuffer<uint8> serialized_data;
    SerializeBuffer(*buffer, &serialized_data);

    binary_archive_manager_->WriteObjectBinary(
        buffer,
        "vertex-buffers.bin",
        serialized_data,
        serialized_data.GetLength());
  }

  void Visit(Skin* skin) {
    Visit(static_cast<NamedObject*>(skin));

    MemoryBuffer<uint8> serialized_data;
    SerializeSkin(*skin, &serialized_data);

    binary_archive_manager_->WriteObjectBinary(
        skin,
        "skins.bin",
        serialized_data,
        serialized_data.GetLength());
  }

  BinaryArchiveManager* binary_archive_manager_;
};

// Checks object has a name that starts with prefix.
// Parameters:
//   object: ObjectBase object to check.
//   prefix: prefix to check for
//   name: optional, pointer to String to store name.
bool NameStartsWithPrefix(const ObjectBase* object,
                          const String& prefix,
                          String* name) {
  String object_name;
  if (object->IsA(NamedObject::GetApparentClass())) {
    object_name = down_cast<const NamedObject*>(object)->name();
  } else if (object->IsA(Param::GetApparentClass())) {
    object_name = down_cast<const Param*>(object)->name();
  }
  if (prefix.size() <= object_name.size() &&
      object_name.compare(0, prefix.size(), prefix) == 0) {
    if (name) {
      *name = object_name;
    }
    return true;
  }
  return false;
}

}  // namespace anonymous

Serializer::Serializer(ServiceLocator* service_locator,
                       StructuredWriter* writer,
                       IArchiveGenerator* archive_generator)
    : class_manager_(service_locator),
      writer_(writer),
      archive_generator_(archive_generator) {
  sections_[PROPERTIES_SECTION].name_ = "properties";
  sections_[PROPERTIES_SECTION].visitor_ = new PropertiesVisitor(writer_);
  sections_[CUSTOM_SECTION].name_ = "custom";
  sections_[CUSTOM_SECTION].visitor_ = new CustomVisitor(
      writer_, &binary_archive_manager_);
  param_visitor_ = new ParamVisitor(writer_);
  binary_visitor_ = new BinaryVisitor(&binary_archive_manager_);
}

Serializer::~Serializer() {
  for (int i = 0; i != NUM_SECTIONS; ++i) {
    delete sections_[i].visitor_;
  }
  delete param_visitor_;
  delete binary_visitor_;
}

void Serializer::SerializePack(Pack* pack) {
  SerializePackBinary(pack);

  writer_->OpenObject();
  writer_->WritePropertyName("version");
  writer_->WriteInt(kSerializerVersion);

  // write out properties for all objects starting with ROOT_PREFIX
  ObjectBaseArray owned_objects = pack->GetByClass<ObjectBase>();
  for (ObjectBaseArray::const_iterator it = owned_objects.begin();
       it != owned_objects.end(); ++it) {
    String name;
    if (NameStartsWithPrefix(*it, ROOT_PREFIX, &name)) {
      writer_->WritePropertyName(name);
      Serialize(writer_, GetObjectId(*it));
    }
  }


  writer_->WritePropertyName("objects");
  writer_->OpenObject();

  std::vector<const ObjectBase::Class*> classes =
      class_manager_->GetAllClasses();

  for (int i = 0; i != classes.size(); ++i) {
    const ObjectBase::Class* current_class = classes[i];
    if (!ObjectBase::ClassIsA(current_class, Param::GetApparentClass())) {
      std::vector<ObjectBase*> objects_of_class;
      for (ObjectBaseArray::const_iterator it = owned_objects.begin();
           it != owned_objects.end(); ++it) {
        if ((*it)->GetClassName() == current_class->name() &&
            !NameStartsWithPrefix(*it, ROOT_PREFIX, NULL)) {
          objects_of_class.push_back(*it);
        }
      }
      if (objects_of_class.size() != 0) {
        writer_->WritePropertyName(current_class->name());
        writer_->OpenArray();
        for (int j = 0; j != objects_of_class.size(); ++j) {
          writer_->OpenObject();
          SerializeObject(objects_of_class[j]);
          writer_->CloseObject();
        }
        writer_->CloseArray();
      }
    }
  }

  writer_->CloseObject();
  writer_->CloseObject();

  binary_archive_manager_.WriteArchive(archive_generator_);
}

void Serializer::SerializePackBinary(Pack* pack) {
  std::vector<ObjectBase*> objects = pack->GetByClass<ObjectBase>();
  for (std::vector<ObjectBase*>::size_type i = 0; i < objects.size(); ++i) {
    binary_visitor_->Accept(objects[i]);
  }
}

void Serializer::SerializeObject(ObjectBase* object) {
  writer_->WritePropertyName("id");
  writer_->WriteInt(object->id());
  for (int i = 0; i != NUM_SECTIONS; ++i) {
    if (sections_[i].visitor_->IsHandled(object->GetClass())) {
      writer_->WritePropertyName(sections_[i].name_);
      writer_->OpenObject();
      SerializeSection(object, static_cast<Section>(i));
      writer_->CloseObject();
    }
  }

  param_visitor_->Accept(object);
}

void Serializer::SerializeSection(ObjectBase* object, Section section) {
  DCHECK(section >= 0 && section < NUM_SECTIONS);
  sections_[section].visitor_->Accept(object);
}

void Serializer::SerializeParam(Param* param) {
  param_visitor_->Accept(param);
}
}  // namespace o3d
